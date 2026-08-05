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
#include <c2go/mlib/glob.h>
#include <c2go/mlib/stdio.h>

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

    mlib_glob_t matches = {0};
    if (mlib_glob("*.c", 0, NULL, &matches) == 0) {
        /* Use matches.gl_pathv[0..matches.gl_pathc). */
        mlib_globfree(&matches);
    }

    mlib_FILE *file = mlib_fopen("result.txt", "w");
    if (file) {
        mlib_fprintf(file, "count=%d\n", count);
        mlib_fclose(file); /* The carrier itself is reclaimed by the GC. */
    }
    mlib_printf("managed stdout is also available\n");
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
#include <c2go/mlib/glob.h>
#include <c2go/mlib/stdio.h>

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

    FILE *file = fopen("result.txt", "w");
    if (file) {
        fprintf(file, "managed stdio\n");
        fclose(file);
    }
    printf("managed stdout is also available\n");
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
rwlocks; the directory-stream lifecycle; `scandir`; Unix `nftw`/`ftw`; and
`glob`; and the first ownership-closed managed `FILE` surface.
Their state algorithms are shared with root libc; only state resolution differs
(direct managed pointer versus unmanaged handle ID). Managed `opendir` and
`fdopendir` allocate the carrier internally with typed `gc_malloc`; `closedir`
clears its state pointer and lets the GC reclaim the carrier. Managed `scandir` allocates
both its result array and each `dirent` on the Go heap. They must remain in
managed storage and are reclaimed by dropping references, not by calling
`free`. Its sort uses typed pointer stores instead of bytewise `qsort`.
The caller must keep the returned pointer graph in managed locals, fields, or
globals; the examples use a managed pragma around the owning code for that
reason.

Managed `nftw` and `ftw` selectively instantiate musl's stateless walk
algorithm over mlib's `DIR` operations; they do not allocate a second state
model. Callback arguments are borrowed views into the active walk frames and
must not be retained after the callback returns.

Managed `glob_t.gl_pathv`, its pointer slots, and every returned string form a
typed GC-owned graph. `GLOB_APPEND` grows it by allocating a new typed vector
and copying pointers with write barriers. `mlib_globfree` clears the carrier's
root for early release or reuse; callers must never pass `gl_pathv` or its
strings to `free`.

Managed `FILE` objects use a typed GC carrier around the shared raw musl stdio
engine. Their buffer, recursive lock, and open-file-list links are direct
managed roots; `fopen` and `fdopen` never allocate with ordinary `malloc`, and
`fclose` retires those roots instead of freeing the carrier. The surface covers
explicit streams plus managed `stdin`/`stdout`/`stderr`: open/close, `fflush`
(including `NULL`), block and character I/O, seek/status, formatted input and
output, `fileno`, and `flockfile`. A package-level finalize hook flushes this
separate FILE world before root libc flushes its own streams. Formatted input
supports the root scanner's conversions, including `%m` and `%p`; `%m` buffers
are allocated by `gc_malloc`, and all pointer results are published with a Go
write barrier. Those results are GC-owned and must not be passed to `free`; a
non-null `%p` result must be a valid Go-managed address, not an arbitrary
foreign address. Managed `getline`/`getdelim` use the same ownership rule: begin
with a null result or reuse a buffer returned by the managed family, and never
pass that buffer to `free`. Managed `fmemopen` retains a caller-supplied
GC-heap buffer for the stream lifetime, or creates GC-owned storage when its
buffer is null; a caller-supplied C stack array is not a valid retained buffer.
`popen`, `open_memstream`, `fopencookie`, and wide stdio are not yet part of
mlib; do not pass an `mlib_FILE *` to their root-libc counterparts.

In unprefixed pthread mode only the synchronization records and functions are
replaced. Thread lifecycle, thread-specific keys, `pthread_once`, and
`pthread_atfork` remain available from the root pthread surface.

The directory propagation cluster is complete. FILE is being extended in
ownership-closed phases; `popen` and the remaining state families retain the
boundaries documented in DESIGN.md.

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
#include <c2go/mlib/glob.h>
#include <c2go/mlib/stdio.h>

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

    mlib_glob_t matches = {0};
    if (mlib_glob("*.c", 0, NULL, &matches) == 0) {
        /* 使用 matches.gl_pathv[0..matches.gl_pathc)。 */
        mlib_globfree(&matches);
    }

    mlib_FILE *file = mlib_fopen("result.txt", "w");
    if (file) {
        mlib_fprintf(file, "count=%d\n", count);
        mlib_fclose(file); /* carrier 本身由 GC 回收。 */
    }
    mlib_printf("managed stdout 同样可用\n");
}

#pragma c2go pop
```

如果希望替换标准 C 名称，必须在第一次包含 mlib 头文件前定义宏：

```c
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/dirent.h>
#include <c2go/mlib/glob.h>
#include <c2go/mlib/stdio.h>

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

    FILE *file = fopen("result.txt", "w");
    if (file) {
        fprintf(file, "managed stdio\n");
        fclose(file);
    }
    printf("managed stdout 同样可用\n");
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
信号量、pthread 的 mutex/condition variable/rwlock、目录流生命周期、
`scandir`、Unix `nftw`/`ftw`、`glob`，以及第一阶段 ownership-closed 的
managed `FILE` 接口。它们的行为核心与根 libc 共用，只有状态解析
方式不同：`mlib` 直接读取 managed pointer，根 libc 仍通过 unmanaged handle ID 查表。managed `opendir`
和 `fdopendir` 在内部使用 typed `gc_malloc` 分配 carrier；`closedir` 清空状态
指针，由 GC 回收 carrier。managed `scandir` 的结果数组及每个 `dirent` 都位于
Go heap，必须保存在 managed storage 中，使用完只需丢弃引用，不能调用 `free`；
排序过程使用 typed pointer store，不使用按字节交换的 `qsort`。调用方必须把返回
的 pointer graph 保存在 managed 局部变量、字段或全局变量中，因此示例在持有这些
对象的代码外使用了 managed pragma。

managed `nftw` 和 `ftw` 在 mlib 的 `DIR` 操作之上选择性实例化 musl 的无状态
遍历算法，不会引入第二套状态模型。回调参数只是当前遍历栈帧的借用视图，回调返回后
不得继续持有。

managed `glob_t.gl_pathv`、其中的 pointer slot 和每个返回字符串共同组成 typed、
GC-owned 的对象图。`GLOB_APPEND` 会分配新的 typed vector，并通过 write barrier
复制旧指针。`mlib_globfree` 会清除 carrier 中的根，便于提前释放或复用；调用方
不能把 `gl_pathv` 或其中的字符串传给 `free`。

managed `FILE` 使用 typed GC carrier 包住共用的 raw musl stdio engine；缓冲区、
递归锁和打开文件链表都由明确的 managed 字段直接持有。`fopen`/`fdopen` 不会调用
普通 `malloc`，`fclose` 清除这些根并由 GC 回收 carrier。当前同时支持显式文件流
和 managed `stdin`/`stdout`/`stderr`：打开/关闭、`fflush`（包括 `NULL`）、块与
字符 I/O、定位与状态、格式化输入输出、`fileno` 和 `flockfile`。包级 finalize
hook 会先刷新这一套独立的 FILE world，再由 root libc 刷新自己的流。格式化输入
支持 root scanner 的全部转换，包括 `%m` 与 `%p`；`%m` 缓冲区由 `gc_malloc`
分配，所有指针结果都通过 Go 写屏障发布。这些结果归 GC 所有，不能传给 `free`；
非空 `%p` 结果必须是有效的 Go managed 地址，不能是任意外部地址。
managed `getline`/`getdelim` 遵循同一所有权规则：传入空结果，或复用该 managed
函数族之前返回的缓冲区，并且不能把结果传给 `free`。
managed `fmemopen` 会在整个 stream 生命周期内保留调用者的 GC heap 缓冲区，
因此不能传入 C 栈数组；传入空缓冲区时，则创建 GC-owned storage。`popen`、
`open_memstream`、`fopencookie` 和 wide stdio 尚未进入 mlib；不能把
`mlib_FILE *` 传给这些根 libc 接口。

pthread 无前缀模式只替换同步对象和同步函数；线程生命周期、线程私有 key、
`pthread_once` 和 `pthread_atfork` 仍由根 pthread 接口提供。

目录传播簇现已完整。FILE 将继续按 ownership-closed 阶段扩展；`popen` 与其余
状态簇仍遵循 DESIGN.md 中记录的边界。
