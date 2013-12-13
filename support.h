#ifndef _TSC_PLATFORM_SUPPORT_H_
#define _TSC_PLATFORM_SUPPORT_H_

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef __APPLE__

#include "pthread_barrier.h"

#define _XOPEN_SOURCE 700

#endif // __APPLE__ 

#define TSC_NAME_LENGTH 32

// -- for thread APIs -- 
typedef pthread_t TSC_OS_THREAD_T;

#define TSC_OS_THREAD_CREATE    pthread_create
#define TSC_OS_THREAD_SENDSIG   pthread_kill

// -- for thread local storages --
#define TSC_TLS_DEFINE \
   pthread_key_t __vpu_keys;

#define TSC_TLS_DECLARE \
    extern TSC_TLS_DEFINE

#define TSC_TLS_INIT() \
    pthread_key_create (& __vpu_keys, NULL)

#define TSC_TLS_SET(p) \
    pthread_setspecific (__vpu_keys, (p))

#define TSC_TLS_GET() \
    pthread_getspecific (__vpu_keys)

// -- for thread barrier --
#define TSC_BARRIER_DEFINE \
    static pthread_barrier_t __vpu_barrier;

#define TSC_BARRIER_INIT(n) \
    pthread_barrier_init (& __vpu_barrier, NULL, n)

#define TSC_BARRIER_WAIT() \
    pthread_barrier_wait (& __vpu_barrier);

// -- for memory alloc api --
#define TSC_ALLOC   malloc
#define TSC_REALLOC realloc
#define TSC_DEALLOC free

// -- for signals --
#define TSC_CLOCK_SIGNAL    SIGUSR1

#define TSC_SIGNAL_MASK_DEFINE      \
    sigset_t __vpu_sigmask;

#define TSC_SIGNAL_MASK_DECLARE     \
    extern TSC_SIGNAL_MASK_DEFINE

#define TSC_SIGNAL_MASK_INIT()   do { \
    sigemptyset (&__vpu_sigmask);        \
    sigaddset (&__vpu_sigmask, SIGUSR1); } while (0)

#ifdef ENABLE_TIMER
# define TSC_SIGNAL_MASK()  \
    sigprocmask (SIG_BLOCK, &__vpu_sigmask, NULL)

# define TSC_SIGNAL_UNMASK() \
    sigprocmask (SIG_UNBLOCK, &__vpu_sigmask, NULL)
#else
# define TSC_SIGNAL_MASK()
# define TSC_SIGNAL_UNMASK()
#endif

#define TSC_RESCHED_THRESHOLD   5

// -- for atomic add op --
#if defined(__APPLE__) && !defined(__i386__) && !defined(__x86_64__)
# define TSC_ATOMIC_INC(n) (++(n))  
// TODO # define TSC_CAS(pval, old, new)  
#else
# define TSC_ATOMIC_INC(n) (__sync_add_and_fetch(&(n), 1))
# define TSC_CAS(pval, old, new) (__sync_bool_compare_and_swap(pval, old, new))
#endif

#if defined(__APPLE__)
# include <sys/param.h>
# include <sys/sysctl.h>
static int inline TSC_NP_ONLINE(void) {
	int num_cpu;
	int mib[4];
	size_t len = sizeof (num_cpu);

	/* set the mib for hw.ncpu */
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;

	/* get the number of CPUs from the system */
	sysctl (mib, 2, & num_cpu, & len, NULL, 0);
	if (num_cpu < 0) {
		mib[1] = HW_NCPU;
		sysctl (mib, 2, & num_cpu, & len, NULL, 0);
		if (num_cpu < 0) return 1;
	}
	return num_cpu;
}
#else // linux platforms ..
# include <sys/sysinfo.h>
# define TSC_NP_ONLINE() get_nprocs()
#endif

#endif // _TSC_PLATFORM_SUPPORT_H_
