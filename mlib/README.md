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
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/dirent.h>

#pragma c2go managed push

/* Public comparators are Go-ABI boundary functions, so wrap one in a local
 * internal-ABI callback before passing it to scandir. */
static int by_name(const struct dirent **a, const struct dirent **b) {
    return mlib_alphasort(a, b);
}

static void example(void) {
    mlib_sem_t sem;
    mlib_sem_init(&sem, 0, 1);

    mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
    mlib_pthread_mutex_lock(&mutex);
    mlib_pthread_mutex_unlock(&mutex);

    mlib_DIR *dir = mlib_opendir(".");
    struct dirent *entry = mlib_readdir(dir);
    mlib_closedir(dir);

    struct dirent **names;
    int count = mlib_scandir(".", &names, NULL, by_name);
    /* Use names[0..count), then drop all references. Do not call free(). */
}

#pragma c2go pop
```

Define `C2GO_MLIB_UNPREFIXED` before the first mlib header to replace the
corresponding standard names for the entire C2Go/LTO package:

```c
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/dirent.h>

#pragma c2go managed push

static void example(void) {
    sem_t sem;
    sem_init(&sem, 0, 1);

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);

    DIR *dir = opendir(".");
    struct dirent *entry = readdir(dir);
    closedir(dir);
}

#pragma c2go pop
```

Do not mix managed and unmanaged definitions of the same standard API in one
LTO package. The unprefixed header must precede the corresponding ordinary
libc header. Namespaced mode may coexist with the ordinary header.

Managed C allocation must be typed and GC-visible:

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

`mlib` does not provide or use `malloc`, `realloc`, or `free`. It currently
implements unnamed semaphores; pthread mutexes, condition variables, and
rwlocks; the directory-stream lifecycle; and `scandir`. Their state algorithms
are shared with root libc; only state resolution differs (direct managed
pointer versus unmanaged handle ID). Managed `opendir` and `fdopendir`
allocate the carrier internally with typed `gc_malloc`; `closedir` clears its
state pointer and lets the GC reclaim the carrier. Managed `scandir` allocates
both its result array and each `dirent` on the Go heap. They must remain in
managed storage and are reclaimed by dropping references, not by calling
`free`. Its sort uses typed pointer stores instead of bytewise `qsort`.
The caller must keep the returned pointer graph in managed locals, fields, or
globals; the examples use a managed pragma around the owning code for that
reason.

In unprefixed pthread mode only the synchronization records and functions are
replaced. Thread lifecycle, thread-specific keys, `pthread_once`, and
`pthread_atfork` remain available from the root pthread surface.

Managed `glob`, `nftw`, and `ftw` are not implemented yet. The unprefixed mlib
headers deliberately do not fall back to root unmanaged versions for these
managed object graphs.

## 简体中文

`mlib` 提供有状态 libc API 的 managed 版本。它的 C 对象直接保存 Go GC
可见的状态指针，不再保存指向进程级 handle table 的整数 ID；根 `libc` 包继续
提供原有 unmanaged 兼容接口。
实现清单和后续迁移边界见 [DESIGN.md](DESIGN.md)。

默认接口带有明确的 `mlib_` 前缀：

```c
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/dirent.h>

#pragma c2go managed push

/* 公共 comparator 是 Go ABI 边界函数，传给 scandir 前要用本地
 * internal-ABI 函数包装。 */
static int by_name(const struct dirent **a, const struct dirent **b) {
    return mlib_alphasort(a, b);
}

static void example(void) {
    mlib_sem_t sem;
    mlib_sem_init(&sem, 0, 1);

    mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
    mlib_pthread_mutex_lock(&mutex);
    mlib_pthread_mutex_unlock(&mutex);

    mlib_DIR *dir = mlib_opendir(".");
    struct dirent *entry = mlib_readdir(dir);
    mlib_closedir(dir);

    struct dirent **names;
    int count = mlib_scandir(".", &names, NULL, by_name);
    /* 使用 names[0..count) 后丢弃引用；不要调用 free()。 */
}

#pragma c2go pop
```

如果希望替换标准 C 名称，必须在第一次包含 mlib 头文件前定义宏：

```c
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/dirent.h>

#pragma c2go managed push

static void example(void) {
    sem_t sem;
    sem_init(&sem, 0, 1);

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);

    DIR *dir = opendir(".");
    struct dirent *entry = readdir(dir);
    closedir(dir);
}

#pragma c2go pop
```

无前缀模式是整个 C2Go/LTO 包的构建选择，不能在同一包内混用 managed 与
unmanaged 的同名标准 API。无前缀 mlib 头文件必须先于普通 libc 对应头文件；
默认带前缀模式则可以与普通头文件共存。

managed C 对象必须通过带类型信息的 GC 分配接口创建：

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

`mlib` 不提供、也不使用 `malloc`、`realloc` 或 `free`。当前已经实现无名
信号量、pthread 的 mutex/condition variable/rwlock、目录流生命周期，以及
`scandir`。它们的行为核心与根 libc 共用，只有状态解析方式不同：`mlib` 直接读取
managed pointer，根 libc 仍通过 unmanaged handle ID 查表。managed `opendir`
和 `fdopendir` 在内部使用 typed `gc_malloc` 分配 carrier；`closedir` 清空状态
指针，由 GC 回收 carrier。managed `scandir` 的结果数组及每个 `dirent` 都位于
Go heap，必须保存在 managed storage 中，使用完只需丢弃引用，不能调用 `free`；
排序过程使用 typed pointer store，不使用按字节交换的 `qsort`。调用方必须把返回
的 pointer graph 保存在 managed 局部变量、字段或全局变量中，因此示例在持有这些
对象的代码外使用了 managed pragma。

pthread 无前缀模式只替换同步对象和同步函数；线程生命周期、线程私有 key、
`pthread_once` 和 `pthread_atfork` 仍由根 pthread 接口提供。

managed `glob`、`nftw` 和 `ftw` 尚未实现。对于这些 managed object graph，
mlib 的无前缀头文件不会静默退回根包的 unmanaged 实现。
