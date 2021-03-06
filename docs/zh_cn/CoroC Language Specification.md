
# CoroC 语言规范 v2.7

- 作者：Amal Cao ([caowei@ict.ac.cn](caowei@ict.ac.cn))
- 发布日期：2016-03-11
- 修改历史：
	* 2014-08-20：初始版本
	* 2014-09-22：将`__time_t`统一替换为`__coroc_time_t`以避免与系统定义类型冲突
	* 2014-10-31：第二版，将原先输出到 C++ 的方案改为直接输出为纯 C 代码 
	* 2014-11-02：修订不能利用`typedef`定义形如`__chan_t<T>`类型的BUG
	* 2014-11-03：增加对定义嵌套`__chan_t`类型时的约束说明
	* 2014-11-25：增加“群组子任务”说明，增加“异步调用”说明
	* 2014-11-26：增加示例4 - file.co 以说明关键字 `__CoroC_Async_Call` 的使用方法
	* 2014-12-22：增加通用“智能指针”功能的说明
	* 2015-03-18：对“智能指针”的功能进行了补充说明
	* 2016-03-11：增加对优先级调度方式的说明
	

## §1 概述

### §1.1  语言说明

CoroC （Coroutine-C） 是基于C99之上构建的一套语法扩展，用来方便C程序员开发基于“协程”的用户级并发系统，同时提供Channel作为用户任务间的通信机制。

### §1.2  编译器命令行参数

目前，针对CoroC语言的编译器仅支持“源代码-源代码”翻译，也就是将某个CoroC源文件进行预处理，进行头文件引入、宏展开等工作，然后输出到一个临时文件中，再对该临时文件作为输入将其翻译成一个同义的C源文件，然后再通过编译这个生成的C文件得到最终的目标文件。

通过clang工具（或clang-co）的 `-rewrite-coroc` 参数，可以将输入的（以".co"作为扩展名的）CoroC源文件翻译成相应的（以“.c”作为扩展名的）C 文件（预编译过程对用户是透明的）。

## §2 语法说明

### §2.1 预编译

CoroC 编译器前端预定义了宏 `__COROC__`，因此可以通过判断该宏是否定义来区别代码的类型，如：


```
#ifdef __COROC__
  // list CoroC code here !!
#else
  // list C code here !!
#endif
```


### §2.2 内建类型与内建常量

#### §2.2.1 内建类型

- `__task_t` -- 用户任务句柄：

	用来引用相应的用户任务（“协程”），可由 `__CoroC_Spawn` 创建用户任务时获得；亦可通过内建函数 `__CoroC_Self()` 返回当前执行的用户任务句柄。（参见[§2.3.1]()）

- `__chan_t` -- Channel句柄：

	用来引用相应的 Channel 对象。通过 `__CoroC_Chan` 创建 Channel 对象时返回，可以函数参数形式进行传递或赋值。**Channel 对象可在用户任务间共享，系统会采用“引用计数”的方式自动对其进行管理。** 
	
	除了用基本形式声明Channel型句柄外，还可以显示指定传输参数类型以便编译器进行静态类型检查，具体的声明方式是 `__chan_t` 后紧接一个尖括号（“<>”）,并在尖括号中指定传输类型。例如：`__chan_t<int> ichan;` (参见[§2.3.2]())
	
	说明：可以通过定义一个宏来简化`__chan_t`的声明形式，如下面的宏可以简化 `__chan_t<int >` 类型的声明，如：`#define ICHAN_T __chan_t<int >`；也支持用`typedef`的方式进行`__chan_t<T >` 的类型简化声明，如：`typedef __chan_t<int> ICHAN_T`。
	
- `__refcnt_t` -- 智能指针类型：

	“智能指针”的主要用于控制动态分配的“堆对象”在不同执行任务间共享时的生命周期管理 —— 简言之，就是提供一种自动“堆对象”释放的机制，当指向某对象的所有引用均失效，则释放该对象。

	由于 CoroC 是基于 C 语言设计的扩展，而在纯 C 中，要实现功能完备的“智能指针”比较困难，因此我们在设计 CoroC 时，将其作为语言特性之一提供给开发者。

	一个指向类型 `T` 的智能指针的定义方式如下：`__refcnt_t<T> smart_ptr;` ，类似之前 `__chan_t` 类型的定义方式，需要在 `<>` 中指明指向的对象类型。**注意，这里的类型 `T` 必须是一个标量类型，如果需要动态分配数组类型，只需给出数组元素类型，具体元素个数由后面具体分配的关键字 `__CoroC_New` 语法指定，请参考后续章节的介绍。**

	类似 C++ 中的实现，在 CoroC 中，“智能指针” 也重载了 "*"、 "->"  以及 "[]" 操作符，分别用来引用指向对象和 `struct` 类型对象的内部元素。此外，我们还为 “智能指针” 设计了一种新的一元操作符 "$"，用来直接获得对象的 C 指针，比如，如果 `ref` 是一个“智能指针”，则 `$ref` 就是其指向第一个元素的地址，这种表述等价于 `&(ref[0])`；同理，`*($ref)` 和 `ref[0]` 等价，都表示第一个元素本身，上述两种方式都可以直接作为左值使用。
	
	同样，开发者也可以利用 `typedef ` 简化智能指针的定义，如：`typedef SP_T __refcnt_t<T>;` 。

- `__coroc_time_t` -- 时间值：

	用来表示系统时间、时刻信息，本质上是一个64位无符号整数。

- `__coroc_group_t` -- 子任务群组：

	 用来将多个子任务划分在同一个群组中，以实现子任务的“创建-规约”操作。（*详见内建函数 [`__CoroC_Group`/`__CoroC_Sync`]() 的相关说明 *）

#### §2.2.2  备注

**CoroC内建类型 `__task_t` 、 `__chan_t` 以及 `__refcnt_t` 均不支持诸如“const”、“volatile”、“static” 这样的类型修饰符，否则将报错（参见[§3.3]() ）！**

**定义传输元素是`__chan_t<T>`类型的另一个`__chan_t`时，需要特别注意一点：内外层元素结尾符`>`间必须间隔至少一个空格符，例如：`__chan_t<__chan_t<long> >`，否则编译器将报错！**

当定义一个“智能指针”时，指向的元素**不可以是另一个“智能指针”类型**，比如定义如下形式类型`__refcnt_t<__refcnt_t<T> > ...` 时，编译器将报错 。

#### §2.2.2 内建常量

- `__CoroC_Null` -- 传输占位符：

	当通过 Channel 操作对某个 Channel 引用进行数据传输时，若传输参数被指定为 `__CoroC_Null` 则表示忽略当前收、发操作的数据。 (参见p[§2.3.2]())



### §2.3 语法

#### §2.3.1 “协程”控制类

##### §2.3.1.1 `__CoroC_Spawn` -- 用户任务创建

- 语法：

```
__task_t task = __CoroC_Spawn func(arg0, arg1, ...);
__CoroC_Spawn func(arg0, arg1, ...);
```

- 功能说明：

	创建一个并发的用户任务并执行指定的函数调用，同时返回该新建任务的句柄（该句柄可以被忽略）。 

- 翻译举例：


```
//	CoroC 代码:
		
int func0(int, __chan_t<int>);

void func1(void) {
	... ...
	int i;
	__chan_t<int> chan = __CoroC_Chan<int>;	
	__CoroC_Spawn func(i, chan);
	... ... 
}
```

	
```
//	对应的C代码：

int func0(int, __chan_t);

/// auto generated by CoroC rewriter.

struct __thunk_struct_1 { 
  int _param0;
  __chan_t _param1;
};

static int __thunk_helper_1(struct __thunk_struct_1 *_arg) {
  func(_arg->_param0, _arg->_param1);
  __CoroC_Exit(0);
}

static int __thunk_cleanup_1(struct __thunk_struct_1 *_arg，int _ret) {
  __refcnt_put(_arg->_param1);
  free(_arg);
  return _ret;
}

/// end

void func1(void) {
  ... ...
  int i;
  __chan_t chan = __CoroC_Chan(sizeof(int), 0);
  {
   struct __thunk_struct_1*  __coroc_temp_0 = (struct __thunk_struct_1*)malloc(sizeof(struct __thunk_struct_1));
   __coroc_temp_0->_param0 = i;
   __coroc_temp_0->_param1 = __refcnt_get(chan);
   __CoroC_Spawn_Opt ((__CoroC_spawn_entry_t)__thunk_helper_1, __coroc_temp_0, (__CoroC_cleanup_cleanup_t)__thunk_cleanup_1);
  }
 ... ...
}

```




##### §2.3.1.2 基于子任务组的 `__CoroC_Spawn`

- 语法：

```
  ... ...
  __group_t grp = __CoroC_Group();
  for (i = 0; i < N; ++i) {
    // spawn the subtasks into a common group 
    __CoroC_Spawn<grp> subtask(arg0, ...);
  }
  // waiting for all subtask done
  __CoroC_Sync(grp);
  // go on 
  ... ... 
```

- 功能说明：
	
	用于子任务管理，在创建子任务时，通过 `__group_t grp` 变量指定子任务所属的任务组；父任务通过调用 `__CoroC_Sync(grp)` 等待所有该组内的子任务执行完成。类似于传统的 "Fork-Join" 模式。
	
- 翻译举例：

```
// CoroC 代码：
#define N   10

int subtask(unsigned *a) {
  *a = rand() % N;
  __CoroC_Quit 0;
}

int main() {
  __group_t grp = __CoroC_Group();
  unsigned i, A[N];

  for (i = 0; i < N; ++i)
    __CoroC_Spawn<grp > subtask(&A[i]);

  __CoroC_Sync(grp);

  __CoroC_Quit 0;
}
```

```
// 对应的 C 代码：
/* C source file auto generated by Clang CoroC rewriter. */
#include <libcoroc.h>


int subtask(unsigned *a) {
  *a = rand() % 10;
  __CoroC_Quit(0);
}


/// auto generated by CoroC rewriter.

struct __thunk_struct_1 { 
  unsigned int * _param0;
  __group_t _group;
};

static int __thunk_helper_1(struct __thunk_struct_1 *_arg) {
  subtask(_arg->_param0);
  __CoroC_Exit(0);
}

static int __thunk_cleanup_1(struct __thunk_struct_1 *_arg, int _ret) {
  __CoroC_Notify(_arg->_group, ret);
  free(_arg);
  return _ret;
}

/// end

int __CoroC_UserMain(int __argc, char **__argv) {
  __group_t grp = __CoroC_Group();
  unsigned i, A[10];

  for (i = 0; i < 10; ++i)
  {
    struct __thunk_struct_1*  __coroc_temp_0 = (struct __thunk_struct_1*)malloc(sizeof(struct __thunk_struct_1));
    __coroc_temp_0->_param0 = &A[i];
    __coroc_temp_0->_group = grp;
    __CoroC_Add_Task(grp);
    __CoroC_Spawn_Opt ((__CoroC_spawn_entry_t)__thunk_helper_1, __coroc_temp_0, (__CoroC_spawn_cleanup_t)__thunk_cleanup_1);
   }

  __CoroC_Sync(grp);

  __CoroC_Quit( 0);
}
... ...
```

##### §2.3.1.2 基于优先级的 `__CoroC_Spawn`

- 语法：

```
__task_t tid = __CoroC_Spawn<priority> func(...); // 单独使用

```

```
__task_t tid = __CoroC_Spawn<group, priority> func(...); // 与 group 混合使用
```

- 功能介绍：
  
  创建基于优先级的任务，其中优先级是一个 0~3 之间的整数，数字越小，优先级越高

- 翻译：

```
// CoroC 代码
void func(int id) {
  printf("my id is %d\n", id);
}

int main(int argc, char **argv) {
  __group_t group = __CoroC_Group();
  int i;
  for (i = 3; i >= 0; --i) {
    __CoroC_Spawn<i, group> func(i);
  }
  
  __CoroC_Sync(group);
  return 0;
}

```

```
// 翻译后的 C 代码
void func(int id) {
  printf("my id is %d\n", id);
}

/// auto generated by CoroC rewriter

struct __thunk_struct_1 {
  int _param0;
  __group_t _group;
};

static int __thunk_helper_1(struct __thunk_struct_1 *_arg) {
  func(_arg->param0);
  __CoroC_Exit(0);
}

static int __thunk_cleanup_1(struct __thunk_struct_1 *_arg, int _ret) {
  __CoroC_Notify(_arg->_group, _ret);
  free(_arg);
  return _ret;
}

/// end

int __CoroC_UserMain(int argc, char** argv) {
  __group_t group = __CoroC_Group();
  int i;

  for (i = 3; i >= 0; i--) {
    {
      struct __thunk_struct_1*  __coroc_temp_0 = (struct __thunk_struct_1*)malloc(sizeof(struct __thunk_struct_1));
      __coroc_temp_0->_param0 = i;
      __coroc_temp_0->_group = group;
      __CoroC_Add_Task(group);
    __CoroC_Spawn((__CoroC_spawn_entry_t)__thunk_helper_1, __coroc_temp_0, i, (__CoroC_spawn_cleanup_t)__thunk_cleanup_1);
    }
  }

  __CoroC_Sync(group);

  return 0;
}
```

##### §2.3.1.4  `__CoroC_Yield` -- 用户任务让出


- 语法：

```
__CoroC_Yield;
```

- 功能说明：

	在任务执行上下文中调用，通知 runtime 调度器当前用户任务主动让出处理器以便其他就绪任务执行。

- 翻译:

	直接翻译为对应的 runtime 函数调用 `__CoroC_Yield();` 。



#####  §2.3.1.5  `__CoroC_Quit` -- 退出当前任务

- 语法：

```
__CoroC_Quit ret;
__CoroC_Quit;
```

- 功能说明：

	在任务执行上下文中调用，通知 runtime 结束当前任务的执行。可选的参数用于指示退出状态，其类型为`int`。


- 翻译:

	直接翻译为对应的 runtime 函数调用 `__CoroC_Quit(ret);`, 若没有指定退出参数，则翻译后的参数为0。
	**【注意】** 由于 `__CoroC_Quit` 语句会引起当前控制流的结束，因此在该语句前，翻译器会生成释放当前函数作用域上所有“智能指针”引用的代码块。


#### §2.3.2 Channel 控制类

##### §2.3.2.1  `__CoroC_Chan` -- 创建Channel

- 语法：

```
__chan_t chan = __CoroC_Chan<typename>;
__chan_t chan = __CoroC_Chan<typename, size>;
```


- 功能说明：

	用于动态创建一个传递数据类型为 `typename` 的 Channel 对象，并返回其句柄。若指定了可选参数 `size`，则该Channel对象的缓冲区大小为`size`；否则该 Channel 对象缓冲区为0。


- 翻译:

	直接翻译为 runtime 函数调用 `__CoroC_Chan(sizeof(typename), 0)` 或 `__CoroC_Chan(sizeof(typename), size)`。由其进行初始化的 `__chan_t` 引用的类型为转化为内建“智能指针”类型，对于当前版本，该智能指针类型名仍为 `__chan_t`。
	
	**注意：如果声明 Channel 句柄时给定了传递类型参数，则其与`__CoroC_Chan`的类型参数必须匹配，否则编译器报错。**


##### §2.3.2.2  `>>` 操作符 -- 读 Channel

- 语法：

```
chan >> i;
chan >> __CoroC_Null;
```

- 功能说明：

	从 `chan` 中读出一个元素并将其内容赋值给相应类型的变量`i`。该操作的返回值是`bool`类型，为`true`表示操作正确执行；否则表示操作出错（`chan`已被关闭）。
	
	**注意：这里要求 ">>" 后跟的变量或表达式必须是一个合法的“左值”，否则编译器将报错。 同时，该变量或表达式的类型必须与`chan`声明的传输类型相同，否则编译器将报错。**


- 翻译:

	直接翻译为 runtime 函数调用，传输表达式或变量作为指针传递，用 "&" 操作符修饰。上面的示例转化后的代码为 `__CoroC_Chan_Recv(chan, &(i));`，若传输表达式为 `__CoroC_Null`，则翻译为: `__CoroC_Chan_Recv(chan, NULL);`，表示忽略接收的具体数值。


##### §2.3.2.3  `<<` 操作符 -- 写 Channel

- 语法：

```
chan << i;
chan << (i + 2)；
chan << __CoroC_Null;
```

- 功能说明：

	向 `chan` 写入一个元素。该操作的返回值是`bool`类型，为`true`表示操作正确执行；否则表示操作出错（`chan`已被关闭）。
	
	**注意：这里要求 "<<" 后跟的变量或表达式**不必**是一个“左值”，但该变量或表达式的类型必须与chan声明的传输类型相同，否则编译器将报错。**


- 翻译:

	若传递参数是一个合法左值，则直接翻译为对应的 runtime 函数调用：`__CoroC_Chan_Send(chan, &(i))`; 否则翻译为：`__CoroC_Chan_SendExpr(chan, (i+2))`;若传输表达式为 `__CoroC_Null`，则翻译为: `__CoroC_Chan_Send(chan, NULL);`，表示忽略发送的具体数值。

- 备注

	目前，Channel 发送操作不支持嵌套调用 Channel 的 << 及 >> 操作，也就是说以下操作是非法的：`chan1 << (chan2 >> i);` ; 
	同时也不支持对未绑定的 Channel 对象进行操作， 故以下操作同样是非法的：`__CoroC_Chan<int > << i;`。


#### §2.3.3 Select 类

Channel 的选择操作属于一类特殊的 Channel 操作，其目的是为了提供对多个 Channel 同时进行监听，并根据最早一个活动 Channel 操作进行相应的分支处理的功能。

##### §2.3.3.1  语法

```
__CoroC_Select {
  __CoroC_Case (ch0 << i0) { /* some code */ }
  __CoroC_Case (ch1 >> i1) { /* some code */ }
  	... ...
  __CoroC_Default { /* some code */ }
} 
```

- 其中，可以包含1个或多个 `__CoroC_Case` 子句和至多1个 `__CoroC_Default` 子句。
- `__CoroC_Case` 后的 Channel 操作必须以"( )"包裹，括号之后的代码块必须用"{ }"包裹。
- `__CoroC_Default` 后不跟 Channel 操作，直接跟代码块，代码块必须以“{ }”包裹。
-  不同 `__CoroC_Case` 子句中的 Channel 操作不能访问同一个 Channel 句柄，否则将抛出运行时错误。

##### §2.3.3.2  包含多个 `__CoroC_Case` 情况的翻译：

若包含多个`__CoroC_Case`， 则翻译为带有 select 操作的代码。

CoroC 代码片段：

```
__CoroC_Select {
	__CoroC_Case(ch0 >> id) { /* code block #0 */ }
	__CoroC_Case(ch1 >> id) { /* code block #1 */ }
	__CoroC_Case(ch2 >> id) { /* code block #2 */ }
	__CoroC_Case(ch3 >> id) { /* code block #3 */ }
	__CoroC_Dedault { /* code block #4 */ }
}
```

翻译后的 C 代码片段：

```
{
	__select_set_t __select_set_2 = __CoroC_Select_Alloc(4);
	__CoroC_Select_Recv(__select_set_2, ch0, &(id));
	__CoroC_Select_Recv(__select_set_2, ch1, &(id));
	__CoroC_Select_Recv(__select_set_2, ch2, &(id));
	__CoroC_Select_Recv(__select_set_2, ch3, &(id));
	__chan_t __select_result_2 = __CoroC_Select(__select_set_2, 1);

	if (ch0 == __select_result_2) { /* code block #0 */ }
	else if (ch1 == __select_result_2) { /* code block #1 */ }
	else if (ch2 == __select_result_2) { /* code block #2 */ }
	else if (ch3 == __select_result_2) { /* code block #3 */ }
	else if (NULL == __select_result_2) { /* code block #4 */ }
	
	__CoroC_Select_Dealloc(__select_set_2);
}
	
```


##### §2.3.3.3  只包含1个 `__CoroC_Case` 情况的翻译：


只包含1个 `__CoroC_Case` 情况下，会翻译为“非阻塞”版的Channel操作，方式如下：

CoroC 代码片段：

```
__CoroC_Select {
	__CoroC_Default { success = 0; }
	__CoroC_Case (ch << i) { success = 1; }
}
```

翻译后的 C 代码片段：

```
{
	bool __select_result_3 = __CoroC_Chan_Recv_NB(ch, &(i));

    if (!__select_result_3) {
	  success = 0;
	}
	else if (__select_result_3) {
	  success = 1;
	}
}
```

#### §2.3.4 异步调用类

由于 CoroC 运行时采用了“协程”作为基本调用单元并且兼容 C 语言语法及函数库，如果在某个“协程”执行过程中调用了一个可能含有阻塞系统调用的操作，如 `fread(...)` / `sleep(...)` 等，该“协程”所在的调度线程就可能被 OS 挂起从而导致整体系统的并行度降低。为了解决这一问题，我们在 CoroC 语言层面提供了异步调用关键字 `__CoroC_Async_Call` 来封装这类阻塞函数调用。

- 语法：

```
__CoroC_Async_Call func(arg0, ...);
T ret = __CoroC_Async_Call func(arg0, ...); 
```

- 功能说明：

对于用户而言，被 `__CoroC_Async_Call` 封装的函数调用和一般的函数调用完全一样，也可以有返回值等。翻译器会把该语句翻译为 CoroC 运行时的“异步请求”接口，阻塞函数将被分发到“异步线程池”等待调度执行，当前“协程”挂起直到阻塞函数执行结束。

【注意】**由于“异步请求”会在非“协程”环境执行，因此需要保证该函数内不存在 CoroC 扩展的仅限于“协程”环境使用的操作，如 Channel 或 Spawn 等。尽量该语句只用于封装兼容 C 库的函数，并且函数中确实存在阻塞系统调用。**


- 翻译：

```
// CoroC 代码：
void func(void) {
  ... ...
  int ret = __CoroC_Async_Call read(fd, buf, N);
  ... ...
}
```

```
// 对应的 C 代码：
/// auto generated by CoroC rewriter.

struct __thunk_struct_1 { 
  int _param0;
  char * _param1;
  int _param2;
  int _ret;
};

static void* __thunk_helper_1(struct __thunk_struct_1 *_arg) {
  _arg->_ret = read(_arg->_param0, _arg->_param1, _arg->_param2);
  return NULL;
}

/// end

void func(void) {
  ... ...
  int ret = ({
    struct __thunk_struct_1*  __coroc_temp_0 = (struct __thunk_struct_1*)alloca(sizeof(struct __thunk_struct_1));
    __coroc_temp_0->_param0 = fd;
    __coroc_temp_0->_param1 = buf;
    __coroc_temp_0->_param2 = 100;
    __CoroC_Async_Call ((__CoroC_async_handler_t)__thunk_helper_1, __coroc_temp_0);
    __coroc_temp_0->_ret; });
  ... ...
}

```

#### §2.3.5 “智能指针” 类

提供一种针对 CoroC “智能指针” 的动态内存分配机制，主要通过关键字 `__CoroC_New` 实现，具体语法如下：

```
extern void destory_struct_T(struct T*);

__refcnt_t<struct T> ref = __CoroC_New<struct T, N, destory_struct_T，AS>;
```

其包含最多 3 个参数：

- 第一个参数是必选的，表示指向对象的类型名称，**必须同 `ref` 声明时指定的元素类型匹配！**
- 第二个参数是可选的，表示分配元素的个数，不填的话默认是 1，**该参数必须是一个大于等于1 的整数表达式！** 当参数大于1时，表示申请一个 `T` 类型的数组，该参数表示元素个数。
- 第三个参数是一个可选的 “析构函数” 的函数名或函数指针，其接受一个类型为 `T *` 的参数，无返回值，用来定制对象内存释放前的一些清理工作。**注意，不要在析构函数中试图调用 `free` 释放对象，就像你不能在 C++ 类析构函数中调用 `free` 释放 `this` 对象一样！系统会在执行完析构函数后自动释放内存！** 另外，**析构函数仅可在申请类型是一个用户定义类型，即 `struct` 或 `union` ,或者是一个指针类型时才能指定，对于基本类型，如 `int` 、`double` 等不支持指定析构函数（没有支持的必要）。**
- 第四个参数也是可选的，用于指明在分配区域后再追加的内存字节数，一般用于分配不定长对象。



### §2.4 内建函数

#### §2.4.1  用户任务类：

- `__task_t __CoroC_Self(void);`

	返回当前执行任务的句柄。
	
- `void __CoroC_Exit(int code);`

	退出当前执行的用户任务，与调用 `__CoroC_Quit` 语句类似。
	
- `bool __CoroC_Task_Send(__task_t target, const void *sendbuf, size_t len);`

	基于任务的发送操作，该操作是非阻塞式的。


- `bool __CoroC_Task_Recv(void *recvbuf, size_t len);`

	基于任务（当前任务）的阻塞式接收操作。

- `bool __CoroC_Task_Recv_NB(void *recvbuf, size_t len);`

	基于任务（当前任务）的非阻塞式接收操作，若没有接收到任何数据，返回 `false`；否则返回 `true`。

	
#### §2.4.2  Channel操作类：

- `void __CoroC_Chan_Close(__chan_t chan);`

	调用该函数将会“关闭”参数指向的 Channel 对象 —— 阻塞在该 Channel 对象的 `<<` 和 `>>` 操作的任务会返回 `false`。

#### §2.4.3  Timer/Ticker 类：


- `__chan_t<__coroc_time_t> __CoroC_Timer_At(__coroc_time_t at); `

	创建一个新的定时器，参数 `at` 为定时器触发的**绝对时刻**；函数返回一个Channel对象的引用，当定时器触发时，该Channel变成可读状态（读取触发时的绝对时刻值）。
	

- `__chan_t<__coroc_time_t> __CoroC_Timer_After(__coroc_time_t after); `

	创建一个新的定时器，参数 `after` 为定时器触发的**相对时刻**；函数返回一个Channel对象的引用，当定时器触发时，该Channel变成可读状态（读取触发时的绝对时刻值）。
	

- `__chan_t<__coroc_time_t> __CoroC_Ticker(__coroc_time_t period);`

	创建一个周期定时器，参数 `period` 指示定时器周期；函数返回一个Channel对象引用，每当定时器周期触发，该Channel变成可读状态（读取触发时的绝对时刻值）。
	
- `void __CoroC_Stop(__chan_t<__coroc_time_t> timer);`
	
	停止一个 Timer/Ticker，注意，传入的参数必须是之前由上述两个函数创建的Channel对象引用，否则将出现未知错误！一旦一个 Timer/Ticker 被终止，则后续对其进行的操作都是非法的，可能引起运行时错误！
	
#### §2.4.4  时间管理类：
	
- `__coroc_time_t __CoroC_Now(void);`

	获取当前系统绝对时刻。
	
- `void __CoroC_Sleep(__coroc_time_t usec);`

	当前任务“休眠” `usec` 微秒。

#### §2.4.5  子任务管理类：	

- `__group_t __CoroC_Group(void);`
	
	初始化一个 `__group_t` 变量，注意，必须在使用 `__CoroC_Spawn<grp>` 和 `__CoroC_Sync(grp)` 前调用此函数对 `__group_t` 类型引用进行初始化操作；一旦执行了一个 `__CoroC_Sync(grp)` 操作后，必须再次调用初始化函数重新初始化方可再次使用该变量。

- `int __CoroC_Sync(__group_t grp);`

	等待属于 `grp` 的全部子任务结束，该操作可能阻塞当前“协程”，知道最后一个子任务结束后，唤醒该阻塞“协程”。返回值表示子任务返回值（即 `__CoroC_Quit` / `__CoroC_Exit` 的返回值）不为0 — 也就是出错的子任务总数；返回值为 0 表示所有子任务均成功。

## §3 编译器错误说明

本节内容仅限由 CoroC 新特性引入的错误。

### §3.1  驱动类错误

- "CoroC does support C language only"
	
	CoroC 仅支持C99上进行扩展，如果当前编译的语言是C++或ObjC、ObjC++等，则会报告该错误。
	
### §3.2  前端错误

- “code-gen action not available for CoroC now”

	CoroC 编译器目前暂不支持直接编译为目标文件，必须通过命令行参数 `-rewrite-coro-c` 先转化为 C 代码，再进行后续目标代码生成的工作。
	

### §3.3  语法分析错误

- “CoroC support disabled”

	在非 CoroC 语言环境下使用了 CoroC 专有的关键字，如 `__CoroC_Spawn` 等。
	
- “found more than one '\_\_CoroC\_Default' in a '\_\_CoroC\_Select' block”

	在一个select块中发现了多个default语句块。
	
- “no '\_\_CoroC_Case' found in a '\_\_CoroC\_Select' block”
	
	在一个select块中没有给出至少一个case语句块。
	
- “the '\_\_chan\_t' and '\_\_task\_t' cannot have any specifiers”

	在声明 CoroC 内建类型 `__chan_t` 和 `__task_t` 时，前面不能添加其他类型修饰符，如 `const`、`static`等，否则将报告该错误。


### §3.4  语义分析错误

#### §3.4.1  任务创建类

- “the argument to '\_\_CoroC\_Spawn' must be a function call”

	`__CoroC_Spawn` 后必须接一个函数调用表达式，否则报告该错误。
	
- “‘\_\_CoroC\_Spawn’ not allowed in this scope”
	
	`__CoroC_Spawn` 出现在非法作用域中。
	
- “builtin function cannot be spawned”

	内建函数不能被作为子任务的启动入口。
	

#### §3.4.2  Channel 类

- “the second param of '\_\_CoroC\_Chan' must be an integer typed expr”

	`__CoroC_Chan` 的第二个表达式必须是一个整型表达式。

- "the expected type is XXX, but the given operand's type is YYY"

	Channel 操作时发现类型匹配错误，Channel对象声明的元素类型和当前操作的元素类型不一致。
	

- "the second operand of the chan recv op must be a lvalue"

	Channel 接收操作时，第二个，也就是 >> 后的那个操作数必须是一个合法的左值。


#### §3.4.3  Select 类

- “the expr of '\_\_CoroC\_Case' must be a channel operation”

	`__CoroC_Case` 后必须紧跟一个Channel操作（`<<` 或 `>>`）,并且该表达式必须被一对括号包围。

- "the body of '\_\_CoroC\_Select' cannot be empty"

	`__CoroC_Select` 后的语句块至少应该包含一个 `__CoroC_Case` 语句。

#### §3.4.4 其他

- “the argument to '\_\_CoroC\_Async\_Call' must be a function call”

	`__CoroC_Async_Call` 后必须接一个函数调用表达式，否则报告该错误。


## §4 程序示例：

由于 CoroC 是基于 C 的语言扩展，因此其完全兼容 C99 的语法，需要注意的一点是在程序开始处，必须首先引入 CoroC 运行时的基础头文件 **libcoroc.h**。下面展示一组简单的示例程序供参考：

### §4.1  示例1 -- mandelbrot.co 

注：本例修改自[benchmarksgame.org](http://benchmarksgame.alioth.debian.org/)上的Go语言版示例。

```
#include <libcoroc.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>


const double ZERO = 0;
const double LIMIT = 2.0;
const int ITER = 50;
const int SIZE = 16000;

uint8_t* rows;
int bytesPerRow;

// This func is responsible for rendering a row of pixels,
// and when complete writing it out to the file.

void renderRow(int w, int h, int bytes, int iter, 
               __chan_t<int> workChan, __chan_t<bool> finishChan) {
  double Zr, Zi, Tr, Ti, Cr, Ci;
  int x, y, i;
  int offset;

  while (workChan >> y) {
    offset = y * bytesPerRow;
    Ci = (2 * (double)y / (double)h - 1.0);

    for (x = 0; x < w; x++) {
      Zr = Zi = Tr = Ti = ZERO;
      Cr = (2 * (double)x / (double)w - 1.5);

      for (i = 0; i < iter && Tr + Ti <= LIMIT * LIMIT; i++) {
        Zi = 2 * Zi * Zr + Ci;
        Zr = Tr - Ti + Cr;
        Tr = Zr * Zr;
        Ti = Zi * Zi;
      }

      // Store tje value in the array of ints
      if (Tr + Ti <= LIMIT * LIMIT)
        rows[offset + x / 8] |= (1 << (uint32_t)(7 - (x % 8)));
    }
  }

  finishChan << __CoroC_Null;
}

#define POOL 4

int main(int argc, char** argv) {
  int size, y, w, h, bytesPerRow, iter;

  size = (argc > 1) ? atoi(argv[1]) : SIZE;

  iter = ITER;
  w = h = size;
  bytesPerRow = w / 8;

  rows = (uint8_t*)malloc(sizeof(uint8_t) * bytesPerRow * h);
  memset(rows, 0, bytesPerRow * h);

  __chan_t workChan = __CoroC_Chan <int, 2*POOL+1>;
  __chan_t finishChan = __CoroC_Chan <bool>;

  for (y = 0; y < size; y++) {
    __CoroC_Spawn renderRow(w, h, bytesPerRow, iter, workChan, finishChan);
    workChan << y;
  }

  __CoroC_Chan_Close(workChan);

  for (y = 0; y < size; y++)
    finishChan >> __CoroC_Null;

  /* -- uncomment the next line to output the result -- */
  /* fwrite (rows, h * w / 8, 1, stdout); */

  free(rows);

  return 0;
}

```

### §4.2  示例2 -- primes.co

注：本例修改自[libtask](http://swtch.com/libtask/)中的相关示例。

```
#include <libcoroc.h>

#include <stdlib.h>
#include <stdio.h>

int quiet = 0;
int goal = 100;
int buffer = 0;

int primetask(__chan_t<unsigned long> c) {
  unsigned long p, i;

  c >> p;

  if (p > goal) exit(0);

  if (!quiet) printf("%lu\n", p);

  __chan_t nc = __CoroC_Chan<unsigned long, buffer>;
  __CoroC_Spawn primetask(nc);

  for (;;) {
    c >> i;
    if (i % p) nc << i;
  }
  return 0;
}

int main(int argc, char **argv) {
  unsigned long i;
  __chan_t c = __CoroC_Chan<unsigned long, buffer>;
  
  printf("goal=%d\n", goal);

  __CoroC_Spawn primetask(c);

  for (i = 2;; i++) 
    c << i;

  return 0;
}
```

### §4.3  示例3 -- ticker.co

本例演示了 CoroC 中定时器的使用方法：

```
#include <libcoroc.h>

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
  __coroc_time_t awaken = 0;
  int i = 0;
  __chan_t<__coroc_time_t> Timer = __CoroC_Ticker(2000000);

  for (i = 0; i < 3; i++) {
    printf("waiting for 2 seconds...\n");
    Timer >> awaken;
    printf("awaken, time is %llu!\n", (long long unsigned)awaken);
  }

  printf("release the timer ..\n");

  __CoroC_Stop(Timer);

  return 0;
}
```

### §4.4  示例4 -- file.co

本例演示 CoroC 中，对 OS 阻塞系统调用的封装机制：

```
#include <unistd.h>
#include <stdio.h>
#include <libcoroc.h>

const char wbuf[] = "Hello the world!\n";
char rbuf[100];

int main(int argc, char **argv) {
  int fd;

  if (argc != 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    __CoroC_Exit(-1);
  }

  fd = __CoroC_Async_Call open(argv[1], O_RDWR | O_APPEND | O_CREAT, 0644);
  __CoroC_Async_Call write(fd, wbuf, sizeof(wbuf));
  __CoroC_Async_Call close(fd);

  fd = open(argv[1], O_RDONLY);
  
  __CoroC_Async_Call read(fd, rbuf, sizeof(wbuf));
  printf("%s\n", rbuf);

  close(fd);

  __CoroC_Quit;
}
```

### §4.5  示例5 -- smart_pointer.co

本例演示 CoroC 中，“智能指针” 的使用方式：

```
#include <stdlib.h>
#include <stdio.h>

#include "libcoroc.h"

struct TreeNode {
  unsigned level;
  __refcnt_t<struct TreeNode> leftTree;
  __refcnt_t<struct TreeNode> rigthTree;
};

void freeTree(struct TreeNode *node) {
  printf("delete tree node level %d..\n", node->level);
}

typedef __refcnt_t<struct TreeNode> TreeNodePtr;

int buildTree(TreeNodePtr *pNode, unsigned level) {
  TreeNodePtr node = __CoroC_New<struct TreeNode, 1, freeTree>;
  
  // using '$' to get the C pointer inside.
  ($node)->level = level;
  if (level < 3) {
    __group_t grp = __CoroC_Group();
    // using '->' / '*' as same as normal C pointers.
    __CoroC_Spawn<grp> buildTree(&(node->leftTree), level+1);
    __CoroC_Spawn<grp> buildTree(&((*node).rigthTree),level+1);

    // wait for the child tasks finish their job..
    __CoroC_Sync(grp);
  }
  
  *pNode = node;
  return 0;
}

int main() {
  TreeNodePtr root;
  buildTree(&root, 0);
  return 0;
}
```

## §5 当前版本缺陷

下面列出当前版本缺陷以及目前的补救方案，供大家参考：

1. 由于采用了静态方式实现“智能指针”方案，因此关键字 `__CoroC_Quit` 或 `__CoroC_Exit()` 等 **NORETURN** 的操作，目前只能释放当前函数作用域内的“智能指针”引用，尚不支持通过栈回溯释放当前调用栈上所有引用的功能，**因此最好使用 `return` 逐层返回**。
1. 利用 clang-coroc 工具直接生成的 C 代码的格式不是很规整，甚至有些混乱， **建议使用 clang-format 等代码格式化工具对输出 C 先进行预处理后再阅读**。
1.  无法对CoroC文件进行直接的代码生成，**必须先输出C文件，再编译该C文件生成目标文件。**

