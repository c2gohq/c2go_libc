/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed FILE carrier over the root package's stateless raw stdio engine.
 * The raw engine stays opaque/noscan; every Go-heap object reachable from
 * it has a parallel managed root in this carrier. */

#include <c2go/mlib/stdio.h>
#include <bits/stdio_impl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

#define MLIB_FILE_RAW_WORDS 32
#define MLIB_FILE_BUFFER_SIZE (UNGET + BUFSIZ)

typedef struct _c2go_mlib_FILE *mlib_file_pointer;
typedef struct _c2go_mlib_FILE *managed mlib_managed_file_pointer;
typedef unsigned char *managed mlib_buffer_pointer;
typedef void *managed mlib_state_pointer;

struct _c2go_mlib_FILE {
    /* A word array gives the raw engine its required alignment without making
     * its internal buffer/callback addresses visible to the Go scanner. */
    uintptr_t _raw[MLIB_FILE_RAW_WORDS];
    mlib_buffer_pointer _buffer_root;
    mlib_state_pointer _lock_state;
    mlib_managed_file_pointer _prev;
    mlib_managed_file_pointer _next;
    int _active;
    int _listed;
};

_Static_assert(sizeof(FILE) <= MLIB_FILE_RAW_WORDS * sizeof(uintptr_t),
    "mlib raw FILE exceeds its managed carrier envelope");

/* Root libc owns the descriptor callbacks and all buffer algorithms. These
 * helpers initialize/close a caller-owned engine without malloc, free, root
 * FILE locks, or root open-file-list membership. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_open", C2GO_GOABI0)
int __c2go_file_raw_open(FILE *, const char *, const char *,
    unsigned char *, size_t);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fdopen", C2GO_GOABI0)
int __c2go_file_raw_fdopen(FILE *, int, const char *, unsigned char *, size_t);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_close", C2GO_GOABI0)
int __c2go_file_raw_close(FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_stdinit", C2GO_GOABI0)
int __c2go_file_raw_stdinit(FILE *, int, unsigned char *, size_t);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_vfscanf_managed", C2GO_GOABI0)
int __c2go_file_raw_vfscanf_managed(FILE *, const char *, va_list);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_vsscanf_managed", C2GO_GOABI0)
int __c2go_vsscanf_managed(const char *, const char *, va_list);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.FileLock", C2GO_GOABI0)
void __c2go_mlib_file_lock(mlib_state_pointer *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.FileTryLock", C2GO_GOABI0)
int __c2go_mlib_file_trylock(mlib_state_pointer *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.FileUnlock", C2GO_GOABI0)
void __c2go_mlib_file_unlock(mlib_state_pointer);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.OpenFileListLock", C2GO_GOABI0)
void __c2go_mlib_ofl_lock(void);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.OpenFileListUnlock", C2GO_GOABI0)
void __c2go_mlib_ofl_unlock(void);

static mlib_managed_file_pointer mlib_ofl_head;
static mlib_managed_file_pointer mlib_std_files[3];

static FILE *mlib_raw(mlib_file_pointer f)
{
    return (FILE *)(void *)f->_raw;
}

__attribute__((noinline))
static mlib_file_pointer mlib_file_allocate(void)
{
    mlib_file_pointer f = gc_malloc(c2go_typeinfo(struct _c2go_mlib_FILE),
        sizeof(*f));
    mlib_buffer_pointer buffer;
    if (!f) {
        errno = ENOMEM;
        return (void *)0;
    }
    buffer = (mlib_buffer_pointer)gc_malloc((void *)0,
        MLIB_FILE_BUFFER_SIZE);
    if (!buffer) {
        errno = ENOMEM;
        return (void *)0;
    }
    f->_buffer_root = buffer;
    return (mlib_FILE *)f;
}

__attribute__((noinline))
static void mlib_stdfile_store(int which, mlib_file_pointer f)
{
    mlib_std_files[which] = (mlib_managed_file_pointer)f;
}

c2go_extern mlib_FILE *mlib_stdfile(int which)
{
    mlib_file_pointer f;
    if ((unsigned)which > 2) {
        errno = EINVAL;
        return (void *)0;
    }

    __c2go_mlib_ofl_lock();
    f = (mlib_file_pointer)mlib_std_files[which];
    if (!f) {
        f = mlib_file_allocate();
        if (f && __c2go_file_raw_stdinit(mlib_raw(f), which,
                (unsigned char *)(void *)f->_buffer_root,
                MLIB_FILE_BUFFER_SIZE) == 0) {
            f->_active = 1;
            mlib_stdfile_store(which, f);
        } else {
            if (f) f->_buffer_root = (void *)0;
            f = (void *)0;
        }
    }
    __c2go_mlib_ofl_unlock();
    return (mlib_FILE *)f;
}

__attribute__((noinline))
static void mlib_ofl_add(mlib_file_pointer f)
{
    __c2go_mlib_ofl_lock();
    f->_prev = (void *)0;
    f->_next = mlib_ofl_head;
    if (mlib_ofl_head)
        mlib_ofl_head->_prev = (mlib_managed_file_pointer)f;
    mlib_ofl_head = (mlib_managed_file_pointer)f;
    f->_listed = 1;
    __c2go_mlib_ofl_unlock();
}

__attribute__((noinline))
static void mlib_clear_file_pointer(mlib_managed_file_pointer *slot)
{
    *slot = (mlib_managed_file_pointer)0;
}

__attribute__((noinline))
static void mlib_file_clear_links(mlib_file_pointer f)
{
    /* Keep each deletion as an individual pointer store. A single inline pair
     * can be widened by LLVM to an integer store before the pointer-barrier
     * pass, erasing its provenance. The noinline helper retains AS1 and the
     * deletion half of Go's hybrid write barrier. */
    mlib_clear_file_pointer(&f->_prev);
    mlib_clear_file_pointer(&f->_next);
}

__attribute__((noinline))
static void mlib_ofl_remove(mlib_file_pointer f)
{
    __c2go_mlib_ofl_lock();
    if (f->_listed) {
        if (f->_prev) f->_prev->_next = f->_next;
        if (f->_next) f->_next->_prev = f->_prev;
        if (mlib_ofl_head == f) mlib_ofl_head = f->_next;
        mlib_file_clear_links(f);
        f->_listed = 0;
    }
    __c2go_mlib_ofl_unlock();
}

/* Returns a locked raw engine, or null with EBADF. The state pointer is kept
 * for the carrier's full lifetime, so waiters can never switch lock instances
 * while fclose is retiring the stream. */
static FILE *mlib_file_acquire(mlib_file_pointer f)
{
    if (!f) {
        errno = EBADF;
        return (void *)0;
    }
    __c2go_mlib_file_lock(&f->_lock_state);
    if (!f->_active) {
        __c2go_mlib_file_unlock(f->_lock_state);
        errno = EBADF;
        return (void *)0;
    }
    return mlib_raw(f);
}

static void mlib_file_release(mlib_file_pointer f)
{
    __c2go_mlib_file_unlock(f->_lock_state);
}

c2go_extern mlib_FILE *mlib_fopen(const char *restrict filename,
                                   const char *restrict mode)
{
    mlib_file_pointer f = mlib_file_allocate();
    if (!f) return (void *)0;
    if (__c2go_file_raw_open(mlib_raw(f), filename, mode,
            (unsigned char *)(void *)f->_buffer_root,
            MLIB_FILE_BUFFER_SIZE) != 0) {
        f->_buffer_root = (void *)0;
        return (void *)0;
    }
    f->_active = 1;
    mlib_ofl_add(f);
    return (mlib_FILE *)f;
}

c2go_extern mlib_FILE *mlib_fdopen(int fd, const char *mode)
{
    mlib_file_pointer f = mlib_file_allocate();
    if (!f) return (void *)0;
    if (__c2go_file_raw_fdopen(mlib_raw(f), fd, mode,
            (unsigned char *)(void *)f->_buffer_root,
            MLIB_FILE_BUFFER_SIZE) != 0) {
        f->_buffer_root = (void *)0;
        return (void *)0;
    }
    f->_active = 1;
    mlib_ofl_add(f);
    return f;
}

c2go_extern int mlib_fclose(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return EOF;
    f->_active = 0;
    result = __c2go_file_raw_close(raw);
    memset(raw, 0, sizeof(*raw));
    f->_buffer_root = (void *)0;
    mlib_file_release(f);
    mlib_ofl_remove(f);
    return result;
}

c2go_extern int mlib_fflush(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw;
    int result, which;
    if (f) {
        raw = mlib_file_acquire(f);
        if (!raw) return EOF;
        result = fflush(raw);
        mlib_file_release(f);
        return result;
    }

    result = 0;
    __c2go_mlib_ofl_lock();
    for (which = 0; which < 3; ++which) {
        f = (mlib_file_pointer)mlib_std_files[which];
        if (!f) continue;
        raw = mlib_file_acquire(f);
        if (raw) {
            result |= fflush(raw);
            mlib_file_release(f);
        }
    }
    for (f = (mlib_file_pointer)mlib_ofl_head; f;
         f = (mlib_file_pointer)f->_next) {
        raw = mlib_file_acquire(f);
        if (raw) {
            result |= fflush(raw);
            mlib_file_release(f);
        }
    }
    __c2go_mlib_ofl_unlock();
    return result;
}

#define MLIB_WRAP_INT_1(name) \
c2go_extern int mlib_##name(mlib_FILE *stream) \
{ \
    mlib_file_pointer f = stream; \
    FILE *raw = mlib_file_acquire(f); \
    int result; \
    if (!raw) return EOF; \
    result = name(raw); \
    mlib_file_release(f); \
    return result; \
}

MLIB_WRAP_INT_1(feof)
MLIB_WRAP_INT_1(ferror)
MLIB_WRAP_INT_1(fgetc)
MLIB_WRAP_INT_1(getc)
MLIB_WRAP_INT_1(fileno)

c2go_extern void mlib_clearerr(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    if (!raw) return;
    clearerr(raw);
    mlib_file_release(f);
}

c2go_extern int mlib_fseek(mlib_FILE *stream, long offset, int whence)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = fseek(raw, offset, whence);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fseeko(mlib_FILE *stream, off_t offset, int whence)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = fseeko(raw, offset, whence);
    mlib_file_release(f);
    return result;
}

c2go_extern long mlib_ftell(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    long result;
    if (!raw) return -1;
    result = ftell(raw);
    mlib_file_release(f);
    return result;
}

c2go_extern off_t mlib_ftello(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    off_t result;
    if (!raw) return -1;
    result = ftello(raw);
    mlib_file_release(f);
    return result;
}

c2go_extern void mlib_rewind(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    if (!raw) return;
    rewind(raw);
    mlib_file_release(f);
}

c2go_extern int mlib_fgetpos(mlib_FILE *restrict stream,
                              mlib_fpos_t *restrict position)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = fgetpos(raw, (fpos_t *)position);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fsetpos(mlib_FILE *stream,
                              const mlib_fpos_t *position)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = fsetpos(raw, (const fpos_t *)position);
    mlib_file_release(f);
    return result;
}

c2go_extern size_t mlib_fread(void *restrict destination, size_t size,
                               size_t count, mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    size_t result;
    if (!raw) return 0;
    result = fread(destination, size, count, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern size_t mlib_fwrite(const void *restrict source, size_t size,
                                size_t count, mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    size_t result;
    if (!raw) return 0;
    result = fwrite(source, size, count, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_ungetc(int character, mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return EOF;
    result = ungetc(character, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fputc(int character, mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return EOF;
    result = fputc(character, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_putc(int character, mlib_FILE *stream)
{
    return mlib_fputc(character, stream);
}

c2go_extern int mlib_getchar(void)
{
    mlib_FILE *stream = mlib_stdfile(0);
    return stream ? mlib_fgetc(stream) : EOF;
}

c2go_extern int mlib_putchar(int character)
{
    mlib_FILE *stream = mlib_stdfile(1);
    return stream ? mlib_fputc(character, stream) : EOF;
}

c2go_extern char *mlib_fgets(char *restrict destination, int count,
                              mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    char *result;
    if (!raw) return (void *)0;
    result = fgets(destination, count, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fputs(const char *restrict text,
                            mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return EOF;
    result = fputs(text, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_puts(const char *text)
{
    mlib_file_pointer f = mlib_stdfile(1);
    FILE *raw;
    int result;
    if (!f) return EOF;
    raw = mlib_file_acquire(f);
    if (!raw) return EOF;
    result = fputs(text, raw);
    if (result >= 0 && fputc('\n', raw) == EOF) result = EOF;
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_vfprintf(mlib_FILE *restrict stream,
                               const char *restrict format, va_list arguments)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = vfprintf(raw, format, arguments);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fprintf(mlib_FILE *restrict stream,
                              const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int mlib_vprintf(const char *restrict format, va_list arguments)
{
    mlib_FILE *stream = mlib_stdfile(1);
    return stream ? mlib_vfprintf(stream, format, arguments) : -1;
}

c2go_extern int mlib_printf(const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vprintf(format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int mlib_vfscanf(mlib_FILE *restrict stream,
                              const char *restrict format, va_list arguments)
{
    mlib_file_pointer f = stream;
    FILE *raw;
    int result;
    raw = mlib_file_acquire(f);
    if (!raw) return EOF;
    result = __c2go_file_raw_vfscanf_managed(raw, format, arguments);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fscanf(mlib_FILE *restrict stream,
                             const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vfscanf(stream, format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int mlib_vscanf(const char *restrict format, va_list arguments)
{
    mlib_FILE *stream = mlib_stdfile(0);
    return stream ? mlib_vfscanf(stream, format, arguments) : EOF;
}

c2go_extern int mlib_scanf(const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vscanf(format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int mlib_vsscanf(const char *restrict input,
                              const char *restrict format, va_list arguments)
{
    return __c2go_vsscanf_managed(input, format, arguments);
}

c2go_extern int mlib_sscanf(const char *restrict input,
                             const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vsscanf(input, format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern void mlib_flockfile(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    if (!f) return;
    __c2go_mlib_file_lock(&f->_lock_state);
}

c2go_extern int mlib_ftrylockfile(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    if (!f) return 0;
    return __c2go_mlib_file_trylock(&f->_lock_state);
}

c2go_extern void mlib_funlockfile(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    if (!f || !f->_lock_state) return;
    __c2go_mlib_file_unlock(f->_lock_state);
}

#pragma c2go pop
