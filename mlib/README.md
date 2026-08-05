# c2go-libc/MLib

[English](#english) | [简体中文](#简体中文)

## English

`mlib` provides managed variants of stateful libc APIs. Its C carriers hold
direct, GC-visible Go pointers instead of integer IDs into process-wide handle
tables. The root `libc` package remains the unmanaged compatibility surface.
The implementation inventory and migration boundaries are recorded in
[DESIGN.md](DESIGN.md).

The default API is explicitly namespaced:

```c
#include <c2go/mlib/semaphore.h>

mlib_sem_t sem;
mlib_sem_init(&sem, 0, 1);

#include <c2go/mlib/pthread.h>
mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
mlib_pthread_mutex_lock(&mutex);
mlib_pthread_mutex_unlock(&mutex);
```

Define `C2GO_MLIB_UNPREFIXED` before the first mlib header to replace the
corresponding standard names for the entire C2Go/LTO package:

```c
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>

sem_t sem;
sem_init(&sem, 0, 1);

#include <c2go/mlib/pthread.h>
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&mutex);
pthread_mutex_unlock(&mutex);
```

Do not mix managed and unmanaged definitions of the same standard API in one
LTO package. The unprefixed header must precede the corresponding ordinary
libc header. Namespaced mode may coexist with the ordinary header.

Managed C allocation must be typed and GC-visible:

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

`mlib` does not provide or use `malloc`, `realloc`, or `free`. It currently
implements unnamed semaphores plus pthread mutexes, condition variables, and
rwlocks. Their state algorithms are shared with root libc; only state
resolution differs (direct managed pointer versus unmanaged handle ID).

In unprefixed pthread mode only the synchronization records and functions are
replaced. Thread lifecycle, thread-specific keys, `pthread_once`, and
`pthread_atfork` remain available from the root pthread surface.

## 简体中文

`mlib` 提供有状态 libc API 的 managed 版本。它的 C 对象直接保存 Go GC
可见的状态指针，不再保存指向进程级 handle table 的整数 ID；根 `libc` 包继续
提供原有 unmanaged 兼容接口。
实现清单和后续迁移边界见 [DESIGN.md](DESIGN.md)。

默认接口带有明确的 `mlib_` 前缀：

```c
#include <c2go/mlib/semaphore.h>

mlib_sem_t sem;
mlib_sem_init(&sem, 0, 1);

#include <c2go/mlib/pthread.h>
mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
mlib_pthread_mutex_lock(&mutex);
mlib_pthread_mutex_unlock(&mutex);
```

如果希望替换标准 C 名称，必须在第一次包含 mlib 头文件前定义宏：

```c
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>

sem_t sem;
sem_init(&sem, 0, 1);

#include <c2go/mlib/pthread.h>
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&mutex);
pthread_mutex_unlock(&mutex);
```

无前缀模式是整个 C2Go/LTO 包的构建选择，不能在同一包内混用 managed 与
unmanaged 的同名标准 API。无前缀 mlib 头文件必须先于普通 libc 对应头文件；
默认带前缀模式则可以与普通头文件共存。

managed C 对象必须通过带类型信息的 GC 分配接口创建：

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

`mlib` 不提供、也不使用 `malloc`、`realloc` 或 `free`。当前已经实现无名
信号量，以及 pthread 的 mutex、condition variable、rwlock。它们的行为核心
与根 libc 共用，只有状态解析方式不同：`mlib` 直接读取 managed pointer，
根 libc 仍通过 unmanaged handle ID 查表。

pthread 无前缀模式只替换同步对象和同步函数；线程生命周期、线程私有 key、
`pthread_once` 和 `pthread_atfork` 仍由根 pthread 接口提供。
