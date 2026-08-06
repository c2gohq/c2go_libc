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
#include <c2go/mlib/regex.h>
#include <c2go/mlib/search.h>
#include <c2go/mlib/string.h>
#include <c2go/mlib/stdio.h>
#include <c2go/mlib/stdlib.h>
#include <c2go/mlib/unistd.h>
#include <c2go/mlib/wchar.h>

#pragma c2go managed push

/* Public comparators are Go-ABI boundary functions, so wrap one in a local
 * internal-ABI callback before passing it to scandir. */
static int by_name(const struct dirent **a, const struct dirent **b) {
    return mlib_alphasort(a, b);
}

static void example(void) {
    char *managed label = mlib_strdup("managed");
    /* Use label, then sever the owning root instead of calling free(). */
    label = NULL;

    char *managed message = NULL;
    if (mlib_asprintf(&message, "managed-%d", 42) >= 0) {
        /* Use message, then retire the GC root explicitly. */
        message = NULL;
    }

    char *managed canonical = mlib_realpath(".", NULL);
    /* Use canonical, then sever the GC root instead of calling free(). */
    canonical = NULL;

    char *managed cwd = mlib_getcwd(NULL, 0);
    /* Managed release is an explicit owner store, even though GC reclaims. */
    cwd = NULL;

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

    mlib_regex_t expression = {0};
    regmatch_t match;
    if (mlib_regcomp(&expression, "^managed$", REG_EXTENDED) == 0) {
        (void)mlib_regexec(&expression, "managed", 1, &match, 0);
        mlib_regfree(&expression);
    }

    mlib_FILE *file = mlib_fopen("result.txt", "w");
    if (file) {
        mlib_fprintf(file, "count=%d\n", count);
        mlib_fclose(file); /* The carrier itself is reclaimed by the GC. */
    }
    mlib_FILE *pipe = mlib_popen("echo managed process", "r");
    if (pipe) mlib_pclose(pipe); /* Close the pipe, then reap the child. */
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
#include <c2go/mlib/regex.h>
#include <c2go/mlib/search.h>
#include <c2go/mlib/string.h>
#include <c2go/mlib/stdio.h>
#include <c2go/mlib/stdlib.h>
#include <c2go/mlib/unistd.h>
#include <c2go/mlib/wchar.h>

#pragma c2go managed push

static void example(void) {
    char *managed label = strdup("managed");
    label = NULL; /* Managed release: never free(label). */

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

Pointer-bearing managed C records must be typed and GC-visible:

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

Pointer-free buffers may use `gc_malloc(NULL, size)`. A type-erased graph such
as TRE still needs an explicit GC-visible owner that roots every block; a
no-scan allocation alone is not an ownership model.

`mlib` does not provide or use `malloc`, `realloc`, or `free`. Include only the
family headers you use (`<c2go/mlib/stdio.h>`, `<c2go/mlib/dirent.h>`, and so
on); there is deliberately no umbrella header while this is a selective
managed surface. It currently
implements unnamed semaphores; pthread lifecycle, thread-specific keys,
mutexes, condition variables, and rwlocks; the directory-stream lifecycle;
`scandir`; Unix `nftw`/`ftw`; `glob`; managed search trees, hash tables, and
queues; and the ownership-closed managed `FILE` surface, including process
streams; managed POSIX regex; plus GC-owned `strdup`, `strndup`, `wcsdup`,
`asprintf`, `vasprintf`, `realpath`, and `getcwd`.
Where state lives in Go, root libc and mlib
share the same behavior core and differ only in state resolution (direct
managed pointer versus unmanaged handle ID). Pointer-bearing C containers are selectively
instantiated with typed GC storage. Managed `opendir` and
`fdopendir` allocate the carrier internally with typed `gc_malloc`; `closedir`
clears its state pointer and lets the GC reclaim the carrier. Managed `scandir` allocates
both its result array and each `dirent` on the Go heap. They must remain in
managed storage and are reclaimed by dropping references, not by calling
`free`. Its sort uses typed pointer stores instead of bytewise `qsort`.
The caller must keep the returned pointer graph in managed locals, fields, or
globals; the examples use a managed pragma around the owning code for that
reason.

Managed `strdup`, `strndup`, and `wcsdup` reuse the root libc's stateless
length/copy algorithms but allocate their results with no-scan `gc_malloc`.
Include `<c2go/mlib/string.h>` for the narrow pair and
`<c2go/mlib/wstring.h>` for `wcsdup`; the broader
`<c2go/mlib/wchar.h>` also includes that declaration. The results are
GC-owned: never pass them to `free`, and assign the last managed owning pointer
to `NULL` after use so the buffer becomes unreachable promptly. In unprefixed
mode the standard C source spellings are preprocessor aliases to the single
`mlib_` implementation; this avoids conflicting LLVM libcall routes in
multi-translation-unit LTO.

Managed `asprintf` and `vasprintf` reuse root libc's stateless `vsnprintf`
formatting engine but allocate the result with no-scan `gc_malloc`. Their
`char **` output must identify a managed pointer slot. The address is used only
during the call, so a managed local in the current frame or a field in a typed
GC record is valid. The implementation clears that slot before doing any work,
leaves it `NULL` after every failure, and publishes a complete result through a
write barrier. Never call `free` on the result; assign its final owner `NULL`.
Replacement mode maps the standard source spellings to the same single
`mlib_` implementation.

Managed `realpath` reuses the root package's cross-platform Go
canonicalisation bridge. With a null output it allocates `PATH_MAX` bytes using
no-scan `gc_malloc`; the result is released by assigning its final owner
`NULL`, never with `free`. Because the standard API conditionally returns either
its input buffer or a new allocation, mlib keeps one precise managed return
type: a non-null caller buffer must also be managed storage such as
`gc_malloc(NULL, PATH_MAX)`, not a C stack array or root `malloc` block. Use root
`realpath` when an ordinary stack buffer is required. Include
`<c2go/mlib/stdlib.h>`; unprefixed mode aliases only `realpath`, leaving the rest
of `<stdlib.h>` on root libc.

Managed `getcwd` follows the same conditional-return rule. A null buffer uses
no-scan `gc_malloc(PATH_MAX)` and ignores `size`, matching the root musl-style
extension. A non-null buffer must be managed storage; use root `getcwd` for a C
stack array. Failed internal allocation or lookup retires the local managed
owner before returning `NULL`, while failure with a caller buffer merely stops
borrowing it and does not consume the caller's owner. Include
`<c2go/mlib/unistd.h>`; unprefixed mode replaces only `getcwd`. The caller must
still assign the final returned owner `NULL` after use.

Managed `nftw` and `ftw` selectively instantiate musl's stateless walk
algorithm over mlib's `DIR` operations; they do not allocate a second state
model. Callback arguments are borrowed views into the active walk frames and
must not be retained after the callback returns.

Managed `glob_t.gl_pathv`, its pointer slots, and every returned string form a
typed GC-owned graph. `GLOB_APPEND` grows it by allocating a new typed vector
and copying pointers with write barriers. `mlib_globfree` clears the carrier's
root for early release or reuse; callers must never pass `gl_pathv` or its
strings to `free`.

Managed POSIX regex selectively instantiates a second TRE engine under private
mlib symbols. Its type-erased internal blocks use no-scan `gc_malloc`, but every
block base is directly rooted by the owning per-`regex_t` Go arena; a separate
temporary arena owns each synchronous match. `regfree` clears the carrier roots
instead of traversing ordinary `free`.

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
Managed `open_memstream` returns GC-owned output and updates it through write
barriers when the buffer grows. Its `char **` and `size_t *` result slots escape
until `fclose`, so they must live in GC-visible long-lived storage (normally a
typed `gc_malloc` record), not in C stack locals; never pass its output to
`free`. Managed `fopencookie` roots a non-null cookie until `fclose`; its cookie
callback parameter is `void *managed`, while callback data buffers and seek
positions are borrowed for the synchronous call only. All callbacks must be
c2go-compiled internal-ABI functions. Wide-stream orientation,
character/string I/O, pushback, and formatted input/output are available through
`<c2go/mlib/wchar.h>` and reuse root libc's lock-free UTF, formatting, and
scanning engines under the managed FILE lock. Wide scanf follows the same `%m`
GC-allocation and `%m`/`%p` write-barrier rules described above. Managed
`popen` keeps its Go process object in a dedicated carrier root and shares root
libc's platform-specific launch, descriptor-transfer, and wait logic without
using `popenTab`. Always close such a stream with managed `pclose`, which closes
the pipe before waiting; plain `fclose` does not reap the process. Never pass an
`mlib_FILE *` to the root-libc process-stream family.

Managed `pthread_t` and `pthread_key_t` are direct GC-visible state pointers.
Thread arguments, return values, and TLS values are managed pointers too, so
they remain live across thread start, join, and destructor execution. In
unprefixed mode lifecycle, keys, and synchronization objects are all replaced;
the integer-only `pthread_once` carrier and stateless `pthread_atfork` remain
shared from the root pthread surface. A successful `pthread_join`,
`pthread_detach`, or `pthread_key_delete` clears the library-owned roots, but
the POSIX by-value signature cannot clear the caller's descriptor; discard it
and preferably assign `NULL` immediately. Never mix root and mlib thread/key
carriers.

Managed `tsearch`/`tfind`/`tdelete`/`twalk`/`tdestroy`, the `hsearch` family,
and `insque`/`remque` retain application pointers in typed GC objects and use
write barriers for insertion, resizing, rotation, and removal. Keys, values,
and queue nodes must live in managed storage. Destruction or removal clears
references and leaves reclamation to the GC; never call `free` on these
containers. Comparators, walk functions, and destroy functions are synchronous
c2go internal-ABI callbacks. `lsearch`/`lfind` keep using the root implementation
because their size-based API erases the element type; use them only with
pointer-free elements.

The handle-table carrier migration is complete for the currently implemented
families, but managed allocation-returning APIs are a separate open inventory.
POSIX regex is now implemented with the arena ownership model described above.
`iconv` intentionally remains root-only because
its required `(iconv_t)-1` value cannot live in a precise-GC pointer slot. The
exact implemented and pending boundary is recorded in DESIGN.md.

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
#include <c2go/mlib/regex.h>
#include <c2go/mlib/string.h>
#include <c2go/mlib/stdio.h>
#include <c2go/mlib/stdlib.h>
#include <c2go/mlib/unistd.h>
#include <c2go/mlib/wchar.h>

#pragma c2go managed push

/* 公共 comparator 是 Go ABI 边界函数，传给 scandir 前要用本地
 * internal-ABI 函数包装。 */
static int by_name(const struct dirent **a, const struct dirent **b) {
    return mlib_alphasort(a, b);
}

static void example(void) {
    char *managed label = mlib_strdup("managed");
    /* 使用后主动断开根，不调用 free()。 */
    label = NULL;

    char *managed message = NULL;
    if (mlib_asprintf(&message, "managed-%d", 42) >= 0) {
        /* 使用后显式清除最后一个 GC root。 */
        message = NULL;
    }

    char *managed canonical = mlib_realpath(".", NULL);
    /* 使用后断开 GC root，不能调用 free()。 */
    canonical = NULL;

    char *managed cwd = mlib_getcwd(NULL, 0);
    /* 即使由 GC 回收，逻辑释放仍显式把 owner 置成 NULL。 */
    cwd = NULL;

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

    mlib_regex_t expression = {0};
    regmatch_t match;
    if (mlib_regcomp(&expression, "^managed$", REG_EXTENDED) == 0) {
        (void)mlib_regexec(&expression, "managed", 1, &match, 0);
        mlib_regfree(&expression);
    }

    mlib_FILE *file = mlib_fopen("result.txt", "w");
    if (file) {
        mlib_fprintf(file, "count=%d\n", count);
        mlib_fclose(file); /* carrier 本身由 GC 回收。 */
    }
    mlib_FILE *pipe = mlib_popen("echo managed process", "r");
    if (pipe) mlib_pclose(pipe); /* 先关闭管道，再回收子进程。 */
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
#include <c2go/mlib/regex.h>
#include <c2go/mlib/string.h>
#include <c2go/mlib/stdio.h>
#include <c2go/mlib/wchar.h>

#pragma c2go managed push

static void example(void) {
    char *managed label = strdup("managed");
    label = NULL; /* managed release：不能 free(label)。 */

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

含指针的 managed C record 必须通过带类型信息的 GC 分配接口创建：

```c
mlib_sem_t *sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*sem));
```

无指针 buffer 可以使用 `gc_malloc(NULL, size)`。TRE 这类 type-erased 对象图还
必须用 GC 可见的 owner 直接持有每一个 block；仅使用 no-scan 分配本身并不构成
所有权模型。

`mlib` 不提供、也不使用 `malloc`、`realloc` 或 `free`。使用者只应包含实际需要的
family header（例如 `<c2go/mlib/stdio.h>`、`<c2go/mlib/dirent.h>`）；在接口仍为
选择性 managed surface 时有意不提供聚合头文件。当前已经实现无名
信号量、pthread 的线程生命周期、线程私有 key、mutex/condition variable/
rwlock、目录流生命周期、`scandir`、Unix `nftw`/`ftw`、`glob`、managed
search tree/hash/queue、包含进程流的 ownership-closed managed `FILE`、managed
POSIX regex，以及 GC-owned `strdup`、`strndup`、`wcsdup`、`asprintf`、
`vasprintf`、`realpath`、`getcwd`。位于 Go 中的状态逻辑
由 root libc 与 mlib 共用，
两者只在状态解析方式上不同：`mlib` 直接读取 managed pointer，根 libc 仍通过
unmanaged handle ID
查表；包含指针的 C 容器则使用 typed GC storage 选择性实例化。managed `opendir`
和 `fdopendir` 在内部使用 typed `gc_malloc` 分配 carrier；`closedir` 清空状态
指针，由 GC 回收 carrier。managed `scandir` 的结果数组及每个 `dirent` 都位于
Go heap，必须保存在 managed storage 中，使用完只需丢弃引用，不能调用 `free`；
排序过程使用 typed pointer store，不使用按字节交换的 `qsort`。调用方必须把返回
的 pointer graph 保存在 managed 局部变量、字段或全局变量中，因此示例在持有这些
对象的代码外使用了 managed pragma。

managed `strdup`、`strndup` 和 `wcsdup` 复用 root libc 的无状态长度/复制算法，
但结果由 no-scan `gc_malloc` 分配。窄字符串接口包含
`<c2go/mlib/string.h>`，单独使用 `wcsdup` 时包含
`<c2go/mlib/wstring.h>`；更完整的 `<c2go/mlib/wchar.h>` 也会包含该声明。
这些结果归 GC 所有，绝不能传给 `free`；最后一次使用后应把持有它的 managed
pointer 主动赋成 `NULL`，让 buffer 及时变成不可达。无前缀模式把标准 C 源码名字
预处理为唯一的 `mlib_` 实现，从而避免多翻译单元 LTO 中产生冲突的 LLVM
libcall route。

managed `asprintf` 和 `vasprintf` 复用 root libc 的无状态 `vsnprintf`
格式化 engine，但结果由 no-scan `gc_malloc` 分配。传入的 `char **` 必须指向
managed pointer slot；函数不会保留该 slot 的地址，因此当前栈帧内的 managed
局部变量或 typed GC record 字段都可以使用。实现会先把 slot 清成 `NULL`，任何
失败路径都保持 `NULL`，只有完整格式化成功后才通过写屏障发布结果。结果不能传给
`free`，最后使用完必须把持有它的 managed pointer 主动赋成 `NULL`。无前缀模式
仍然把标准源码名称映射到唯一一份 `mlib_` 实现。

managed `realpath` 复用 root package 的跨平台 Go 路径规范化 bridge。
输出参数为空时，它使用 no-scan `gc_malloc` 分配 `PATH_MAX` 字节；结果最后通过把
owner 赋成 `NULL` 释放，不能调用 `free`。标准接口会根据参数返回调用者 buffer 或
新分配结果，为了让返回指针始终保持精确的 managed 类型，mlib 要求非空调用者
buffer 也必须来自 `gc_malloc(NULL, PATH_MAX)` 等 managed storage，不能使用 C
栈数组或 root `malloc`。需要普通栈 buffer 时应继续调用 root `realpath`。接口位于
`<c2go/mlib/stdlib.h>`；无前缀模式只替换 `realpath`，其余 `<stdlib.h>` API 仍由
root libc 提供。

managed `getcwd` 使用同样的条件返回规则。buffer 为空时，它通过 no-scan
`gc_malloc(PATH_MAX)` 分配结果，并与 root 的 musl 风格扩展一样忽略 `size`；
buffer 非空时必须传 managed storage，需要 C 栈数组时应使用 root `getcwd`。
内部新分配或路径读取失败时会先清除局部 managed owner；调用者 buffer 失败时只会
结束借用，不会消耗调用者已有的 owner。接口位于 `<c2go/mlib/unistd.h>`，无前缀
模式只替换 `getcwd`。最终返回结果用完后仍必须把最后一个 owner 赋成 `NULL`。

managed `nftw` 和 `ftw` 在 mlib 的 `DIR` 操作之上选择性实例化 musl 的无状态
遍历算法，不会引入第二套状态模型。回调参数只是当前遍历栈帧的借用视图，回调返回后
不得继续持有。

managed `glob_t.gl_pathv`、其中的 pointer slot 和每个返回字符串共同组成 typed、
GC-owned 的对象图。`GLOB_APPEND` 会分配新的 typed vector，并通过 write barrier
复制旧指针。`mlib_globfree` 会清除 carrier 中的根，便于提前释放或复用；调用方
不能把 `gl_pathv` 或其中的字符串传给 `free`。

managed POSIX regex 会在私有 mlib 符号下选择性实例化第二份 TRE engine。
内部 type-erased block 使用 no-scan `gc_malloc`，但每个 block 的基地址都由该
`regex_t` 的 Go arena 直接持有；每次同步匹配另有临时 arena。`regfree` 只清除
carrier 根，不会逐项调用普通 `free`。

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
因此不能传入 C 栈数组；传入空缓冲区时，则创建 GC-owned storage。
managed `open_memstream` 返回 GC-owned 输出，扩容时通过写屏障重新发布指针。
它会一直保留 `char **` 与 `size_t *` 两个结果槽直到 `fclose`，所以结果槽必须
位于 GC 可见且寿命足够长的存储中（通常是 typed `gc_malloc` record 的字段），
不能是 C 栈局部变量；输出也不能传给 `free`。
managed `fopencookie` 会保留非空 cookie 直到 `fclose`；回调的 cookie 参数是
`void *managed`，数据缓冲区与 seek position 只在同步回调期间借用，不能逃逸。
所有回调都必须是由 c2go 编译的 internal-ABI 函数。
`<c2go/mlib/wchar.h>` 已提供 wide stream 定向、字符/字符串读写、回退和格式化
输入输出，并在 managed FILE 锁内复用 root libc 的无锁 UTF、格式化与扫描
engine。wide scanf 遵循上文相同的 `%m` GC 分配及 `%m`/`%p` 写屏障规则。
managed `popen` 会把 Go 进程对象直接保存在 carrier 的专用 managed root 中，
并复用 root libc 的跨平台启动、描述符交接与 wait 逻辑，不经过 `popenTab`。
进程流必须用 managed `pclose` 关闭：它先关闭管道，再等待并回收子进程；普通
`fclose` 不负责回收子进程。不能把 `mlib_FILE *` 传给根 libc 的进程流接口。

managed `pthread_t` 和 `pthread_key_t` 都是 GC 可见的直接状态指针；线程参数、
返回值和 TLS value 也显式声明为 managed pointer，因此能跨线程启动、join 和
析构过程保活。无前缀模式会同时替换生命周期、key 与同步对象；只包含整数状态的
`pthread_once` carrier 和无状态的 `pthread_atfork` 继续复用根 pthread 接口。
`pthread_join`、`pthread_detach` 或 `pthread_key_delete` 成功后，库内持有的根
会被清除；但 POSIX 接口按值接收 descriptor，无法替调用者修改变量，因此调用者
应立即丢弃并最好主动赋成 `NULL`。不能在 root 与 mlib 之间混用 thread/key
carrier。

managed `tsearch`/`tfind`/`tdelete`/`twalk`/`tdestroy`、`hsearch` family 和
`insque`/`remque` 会把应用指针保存在 typed GC 对象中，并通过写屏障完成插入、
扩容、旋转和移除。key、value 与 queue node 必须位于 managed storage；销毁或
移除只会清除引用，由 GC 回收，不能调用 `free`。comparator、walk 与 destroy
回调必须是 c2go 编译的同步 internal-ABI 函数。`lsearch`/`lfind` 的 `size_t`
接口擦除了元素类型，无法生成 pointer bitmap，因此仍复用 root 版本且只允许
不含指针的元素。

当前已实现 family 的 handle-table carrier 迁移已经闭环，但返回 managed 分配的
接口是另一份仍开放的清单。POSIX regex 已按上面的 arena ownership 模型实现。
`iconv` 是有意保留的
root-only 例外：它要求的 `(iconv_t)-1` 不能放进 precise-GC pointer slot。准确的
已实现与待迁移边界见 DESIGN.md。
