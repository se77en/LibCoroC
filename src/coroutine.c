// Copyright 2016 Amal Cao (amalcaowei@gmail.com). All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE.txt file.

#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "coroutine.h"
#include "vpu.h"
#include "coroc_group.h"

#if defined(ENABLE_SPLITSTACK)  // && defined(LINKER_SUPPORTS_SPLIT_STACK)
#define TSC_DEFAULT_STACK_SIZE PTHREAD_STACK_MIN
#else
#ifdef __APPLE__
#define TSC_DEFAULT_STACK_SIZE (2 * 1024 * 1024)  // 2MB
#else
#define TSC_DEFAULT_STACK_SIZE (1 * 1024 * 1024)  // 1MB
#endif
#endif

#define TSC_BACKTRACE_LEVEL 20

#define TSC_DEFAULT_AFFINITY (vpu_manager.xt_index)
#define TSC_DEFAULT_TIMESLICE 5
#define TSC_DEFAULT_DETACHSTATE TSC_COROUTINE_UNDETACH

TSC_TLS_DECLARE
TSC_SIGNAL_MASK_DECLARE

void coroc_coroutine_attr_init(coroc_coroutine_attributes_t *attr) {
  attr->stack_size = TSC_DEFAULT_STACK_SIZE;
  attr->timeslice = TSC_DEFAULT_TIMESLICE;
  attr->affinity = TSC_DEFAULT_AFFINITY;
}

coroc_coroutine_t coroc_coroutine_allocate(coroc_coroutine_handler_t entry,
                                       void *arguments, const char *name,
                                       uint32_t type, unsigned priority,
                                       coroc_coroutine_cleanup_t cleanup) {
  assert(priority < TSC_PRIO_NUM);

  TSC_SIGNAL_MASK();

  size_t size;
  vpu_t *vpu = TSC_TLS_GET();
  coroc_coroutine_t coroutine = TSC_ALLOC(sizeof(struct coroc_coroutine));
  memset(coroutine, 0, sizeof(struct coroc_coroutine));
  memset(&coroutine->ctx, 0, sizeof(TSC_CONTEXT));

  if (coroutine != NULL) {
    // init the interal channel ..
    coroc_async_chan_init((coroc_async_chan_t)coroutine);

    strcpy(coroutine->name, name);
    coroutine->type = type;
    coroutine->status = TSC_COROUTINE_WAIT;
    coroutine->async_wait = 0;
    coroutine->id = TSC_ALLOC_TID();
    coroutine->entry = entry;
    coroutine->arguments = arguments;
    coroutine->cleanup = cleanup;
    coroutine->syscall = false;
    coroutine->sigmask_nest = 0;

    if (type == TSC_COROUTINE_IDLE) {
      coroutine->stack_size = 0;
      coroutine->priority = TSC_PRIO_NUM; // lower than default
    } else {
      coroutine->stack_size = TSC_DEFAULT_STACK_SIZE;
      coroutine->priority = priority < TSC_PRIO_NUM ? 
                                priority : TSC_PRIO_LOW;
    }

    coroutine->vpu_affinity = TSC_DEFAULT_AFFINITY;
    coroutine->init_timeslice = TSC_DEFAULT_TIMESLICE;

    if (coroutine->stack_size > 0) {
#ifdef ENABLE_SPLITSTACK
      coroutine->stack_base =
          TSC_STACK_CONTEXT_MAKE(coroutine->stack_size, &coroutine->ctx, &size);
#else
      size = coroutine->stack_size;
      coroutine->stack_base = TSC_ALLOC(coroutine->stack_size);
#endif
      assert(coroutine->stack_base != NULL);
    }
    coroutine->rem_timeslice = coroutine->init_timeslice;

    queue_item_init(&coroutine->status_link, coroutine);
    queue_item_init(&coroutine->trace_link, coroutine);
  }

  if (coroutine->type != TSC_COROUTINE_IDLE) {
    TSC_CONTEXT_INIT(&coroutine->ctx, coroutine->stack_base, size, coroutine);
    atomic_queue_add(&vpu_manager.coroutine_list, &coroutine->trace_link);

    if (coroutine->type == TSC_COROUTINE_MAIN) {
      vpu_manager.main = coroutine;
    }

    // add the `main' task into global runq:
    vpu_ready(coroutine, true);
  }
  TSC_SIGNAL_UNMASK();
  return coroutine;
}

void coroc_coroutine_deallocate(coroc_coroutine_t coroutine) {
  assert(coroutine->status == TSC_COROUTINE_RUNNING);
  // TODO : reclaim the coroutine elements ..
  coroutine->status = TSC_COROUTINE_EXIT;
  atomic_queue_extract(&vpu_manager.coroutine_list, &coroutine->trace_link);

#ifdef ENABLE_SPLITSTACK
  __splitstack_releasecontext(&coroutine->ctx.stack_ctx[0]);
#else
  if (coroutine->stack_size > 0) {
    TSC_DEALLOC(coroutine->stack_base);
    coroutine->stack_base = 0;
  }
#endif

  coroc_async_chan_fini((coroc_async_chan_t)coroutine);
  coroc_refcnt_put((coroc_refcnt_t)coroutine);
  // TSC_DEALLOC(coroutine);
}

void coroc_coroutine_exit(int value) {
  TSC_SIGNAL_MASK();
  
  // call notify API before quit,
  // if current task belongs to a group!!
  vpu_t *vpu = TSC_TLS_GET();
  coroc_coroutine_t self = vpu->current;
  
  assert(self && (self->status == TSC_COROUTINE_RUNNING));

  if (self && self->cleanup)
    self->cleanup(self->arguments, value);

  vpu_syscall(core_exit);
  TSC_SIGNAL_UNMASK();
}

void coroc_coroutine_yield(void) {
  TSC_SIGNAL_MASK();
  vpu_syscall(core_yield);
  TSC_SIGNAL_UNMASK();
}

coroc_coroutine_t coroc_coroutine_self(void) {
  coroc_coroutine_t self = NULL;
  TSC_SIGNAL_MASK();
  vpu_t *vpu = TSC_TLS_GET();

  self = (vpu != NULL) ? vpu->current : NULL;
  TSC_SIGNAL_UNMASK();

  return self;
}

void coroc_coroutine_backtrace(coroc_coroutine_t self) {
  int level;
  void *buffer[TSC_BACKTRACE_LEVEL];

  TSC_SIGNAL_MASK();

  fprintf(stderr, "The \"%s\"<#%d> coroutine's stack trace:\n", self->name,
          self->id);

  level = backtrace(buffer, TSC_BACKTRACE_LEVEL);
  backtrace_symbols_fd(buffer, level, STDERR_FILENO);

  fprintf(stderr, "\n");

  vpu_syscall(core_exit);
  TSC_SIGNAL_UNMASK();
}

