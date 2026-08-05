/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed FILE carrier over the root package's stateless raw stdio engine.
 * The raw engine stays opaque/noscan; every Go-heap object reachable from
 * it has a parallel managed root in this carrier. */

#include <c2go/mlib/wchar.h>
#include <bits/stdio_impl.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

#define MLIB_FILE_RAW_WORDS 32
#define MLIB_FILE_COOKIE_WORDS 8
#define MLIB_FILE_BUFFER_SIZE (UNGET + BUFSIZ)

typedef struct _c2go_mlib_FILE *mlib_file_pointer;
typedef struct _c2go_mlib_FILE *managed mlib_managed_file_pointer;
typedef unsigned char *managed mlib_buffer_pointer;
typedef void *managed mlib_state_pointer;
typedef char *managed mlib_line_pointer;
typedef mlib_line_pointer *managed mlib_line_slot;
typedef size_t *managed mlib_size_slot;

struct _c2go_mlib_FILE {
    /* A word array gives the raw engine its required alignment without making
     * its internal buffer/callback addresses visible to the Go scanner. */
    uintptr_t _raw[MLIB_FILE_RAW_WORDS];
    uintptr_t _cookie_raw[MLIB_FILE_COOKIE_WORDS];
    mlib_buffer_pointer _buffer_root;
    mlib_state_pointer _object_root;
    mlib_state_pointer _result_pointer_slot;
    mlib_state_pointer _result_size_slot;
    mlib_state_pointer _lock_state;
    size_t _memory_pos;
    size_t _memory_len;
    size_t _memory_space;
    mlib_managed_file_pointer _prev;
    mlib_managed_file_pointer _next;
    int _active;
    int _listed;
};

_Static_assert(sizeof(FILE) <= MLIB_FILE_RAW_WORDS * sizeof(uintptr_t),
    "mlib raw FILE exceeds its managed carrier envelope");
_Static_assert(offsetof(struct _c2go_mlib_FILE, _raw) == 0,
    "mlib raw FILE must remain the carrier prefix");
_Static_assert(sizeof(mlib_cookie_io_functions_t) <=
        MLIB_FILE_COOKIE_WORDS * sizeof(uintptr_t),
    "mlib cookie callbacks exceed their raw envelope");

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
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fmemopen", C2GO_GOABI0)
int __c2go_file_raw_fmemopen(FILE *, void *, size_t, unsigned char *, size_t,
    void *, size_t, const char *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_vfscanf_managed", C2GO_GOABI0)
int __c2go_file_raw_vfscanf_managed(FILE *, const char *, va_list);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_vsscanf_managed", C2GO_GOABI0)
int __c2go_vsscanf_managed(const char *, const char *, va_list);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_getdelim_managed", C2GO_GOABI0)
ssize_t __c2go_file_raw_getdelim_managed(mlib_line_slot, mlib_size_slot,
    int, FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fwide", C2GO_GOABI0)
int __c2go_file_raw_fwide(FILE *, int);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fputwc", C2GO_GOABI0)
wint_t __c2go_file_raw_fputwc(wchar_t, FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fgetwc", C2GO_GOABI0)
wint_t __c2go_file_raw_fgetwc(FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fputws", C2GO_GOABI0)
int __c2go_file_raw_fputws(const wchar_t *, FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_fgetws", C2GO_GOABI0)
wchar_t *__c2go_file_raw_fgetws(wchar_t *, int, FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_ungetwc", C2GO_GOABI0)
wint_t __c2go_file_raw_ungetwc(wint_t, FILE *);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_file_raw_vfwprintf", C2GO_GOABI0)
int __c2go_file_raw_vfwprintf(FILE *, const wchar_t *, va_list);

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
static void mlib_clear_buffer_pointer(mlib_buffer_pointer *slot)
{
    *slot = (mlib_buffer_pointer)0;
}

__attribute__((noinline))
static void mlib_clear_state_pointer(mlib_state_pointer *slot)
{
    *slot = (mlib_state_pointer)0;
}

__attribute__((noinline))
static void mlib_store_state_pointer(mlib_state_pointer *slot,
                                     mlib_state_pointer value)
{
    *slot = value;
}

__attribute__((noinline))
static void mlib_store_line_pointer(mlib_line_slot slot,
                                    mlib_line_pointer value)
{
    *slot = value;
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

c2go_extern mlib_FILE *mlib_fmemopen(void *restrict buffer, size_t size,
                                     const char *restrict mode)
{
    mlib_file_pointer f = mlib_file_allocate();
    if (!f) return (void *)0;
    if (!buffer) {
        buffer = gc_malloc((void *)0, size ? size : 1);
        if (!buffer) {
            mlib_clear_buffer_pointer(&f->_buffer_root);
            errno = ENOMEM;
            return (void *)0;
        }
    }
    mlib_store_state_pointer(&f->_object_root, (mlib_state_pointer)buffer);
    if (__c2go_file_raw_fmemopen(mlib_raw(f), f->_cookie_raw,
            sizeof(f->_cookie_raw), (unsigned char *)(void *)f->_buffer_root,
            MLIB_FILE_BUFFER_SIZE, buffer, size, mode) != 0) {
        mlib_clear_state_pointer(&f->_object_root);
        mlib_clear_buffer_pointer(&f->_buffer_root);
        return (void *)0;
    }
    f->_active = 1;
    mlib_ofl_add(f);
    return f;
}

static off_t mlib_memstream_seek(FILE *raw, off_t offset, int whence)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    ssize_t base;
    if (whence > 2U) {
fail:
        errno = EINVAL;
        return -1;
    }
    base = (size_t [3]){0, f->_memory_pos, f->_memory_len}[whence];
    if (offset < -base || offset > SSIZE_MAX-base) goto fail;
    return f->_memory_pos = base + offset;
}

static size_t mlib_memstream_write(FILE *raw, const unsigned char *source,
                                   size_t length)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    mlib_line_pointer current;
    mlib_line_pointer next;
    mlib_line_slot result_slot;
    mlib_size_slot size_slot;
    size_t buffered = raw->wpos - raw->wbase;
    size_t required, new_space;

    if (buffered) {
        raw->wpos = raw->wbase;
        if (mlib_memstream_write(raw, raw->wbase, buffered) < buffered)
            return 0;
    }
    if (length > SIZE_MAX - f->_memory_pos - 1) {
        errno = ENOMEM;
        return 0;
    }
    required = f->_memory_pos + length + 1;
    current = (mlib_line_pointer)f->_object_root;
    result_slot = (mlib_line_slot)f->_result_pointer_slot;
    size_slot = (mlib_size_slot)f->_result_size_slot;
    if (required > f->_memory_space) {
        if (f->_memory_space <= (SIZE_MAX - 1) / 2)
            new_space = 2 * f->_memory_space + 1;
        else
            new_space = required;
        if (new_space < required) new_space = required;
        next = (mlib_line_pointer)gc_malloc((void *)0, new_space);
        if (!next) return 0;
        if (f->_memory_space)
            memcpy((void *)next, (const void *)current, f->_memory_space);
        mlib_store_state_pointer(&f->_object_root,
            (mlib_state_pointer)next);
        mlib_store_line_pointer(result_slot, next);
        f->_memory_space = new_space;
        current = next;
    }
    memcpy((void *)(current + f->_memory_pos), source, length);
    f->_memory_pos += length;
    if (f->_memory_pos >= f->_memory_len) f->_memory_len = f->_memory_pos;
    *size_slot = f->_memory_pos;
    return length;
}

static int mlib_memstream_close(FILE *raw)
{
    (void)raw;
    return 0;
}

c2go_extern mlib_FILE *mlib_open_memstream(char **buffer_slot,
                                            size_t *size_slot)
{
    mlib_file_pointer f;
    mlib_line_pointer buffer;
    FILE *raw;
    if (!buffer_slot || !size_slot) {
        errno = EINVAL;
        return (void *)0;
    }
    f = mlib_file_allocate();
    if (!f) return (void *)0;
    buffer = (mlib_line_pointer)gc_malloc((void *)0, 1);
    if (!buffer) {
        mlib_clear_buffer_pointer(&f->_buffer_root);
        errno = ENOMEM;
        return (void *)0;
    }

    mlib_store_state_pointer(&f->_object_root, (mlib_state_pointer)buffer);
    mlib_store_state_pointer(&f->_result_pointer_slot,
        (mlib_state_pointer)buffer_slot);
    mlib_store_state_pointer(&f->_result_size_slot,
        (mlib_state_pointer)size_slot);
    mlib_store_line_pointer((mlib_line_slot)buffer_slot, buffer);
    *size_slot = 0;
    f->_memory_space = 1;

    raw = mlib_raw(f);
    memset(raw, 0, sizeof(*raw));
    raw->flags = F_NORD;
    raw->fd = -1;
    raw->buf = (unsigned char *)(void *)f->_buffer_root + UNGET;
    raw->buf_size = BUFSIZ;
    raw->lbf = EOF;
    raw->write = mlib_memstream_write;
    raw->seek = mlib_memstream_seek;
    raw->close = mlib_memstream_close;
    raw->mode = -1;
    raw->lock = -1;

    f->_active = 1;
    mlib_ofl_add(f);
    return f;
}

static mlib_cookie_io_functions_t *mlib_cookie_functions(mlib_file_pointer f)
{
    return (mlib_cookie_io_functions_t *)(void *)f->_cookie_raw;
}

static size_t mlib_cookie_read(FILE *raw, unsigned char *buffer, size_t length)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    mlib_cookie_io_functions_t *functions = mlib_cookie_functions(f);
    ssize_t result = -1;
    size_t remaining = length, read_length = 0;
    size_t direct_length = length - !!raw->buf_size;

    if (!functions->read) goto fail;
    if (direct_length) {
        result = functions->read(f->_object_root, (char *)buffer,
                                 direct_length);
        if (result <= 0) goto fail;
        read_length += result;
        remaining -= result;
    }
    if (!raw->buf_size || remaining > !!raw->buf_size) return read_length;

    raw->rpos = raw->buf;
    result = functions->read(f->_object_root, (char *)raw->rpos,
                             raw->buf_size);
    if (result <= 0) goto fail;
    raw->rend = raw->rpos + result;
    buffer[read_length++] = *raw->rpos++;
    return read_length;

fail:
    raw->flags |= result == 0 ? F_EOF : F_ERR;
    raw->rpos = raw->rend = raw->buf;
    return read_length;
}

static size_t mlib_cookie_write(FILE *raw, const unsigned char *buffer,
                                size_t length)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    mlib_cookie_io_functions_t *functions = mlib_cookie_functions(f);
    ssize_t result;
    size_t buffered = raw->wpos - raw->wbase;

    if (!functions->write) return length;
    if (buffered) {
        raw->wpos = raw->wbase;
        if (mlib_cookie_write(raw, raw->wpos, buffered) < buffered) return 0;
    }
    result = functions->write(f->_object_root, (const char *)buffer, length);
    if (result < 0) {
        raw->wpos = raw->wbase = raw->wend = (void *)0;
        raw->flags |= F_ERR;
        return 0;
    }
    return result;
}

static off_t mlib_cookie_seek(FILE *raw, off_t offset, int whence)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    mlib_cookie_io_functions_t *functions = mlib_cookie_functions(f);
    int result;

    if (whence > 2U) {
        errno = EINVAL;
        return -1;
    }
    if (!functions->seek) {
        errno = ENOTSUP;
        return -1;
    }
    result = functions->seek(f->_object_root, &offset, whence);
    if (result < 0) return result;
    return offset;
}

static int mlib_cookie_close(FILE *raw)
{
    mlib_file_pointer f = (mlib_file_pointer)(void *)raw;
    mlib_cookie_io_functions_t *functions = mlib_cookie_functions(f);
    if (functions->close) return functions->close(f->_object_root);
    return 0;
}

c2go_extern mlib_FILE *mlib_fopencookie(
    void *managed cookie, const char *mode,
    mlib_cookie_io_functions_t functions)
{
    mlib_file_pointer f;
    FILE *raw;

    if (!mode || !strchr("rwa", *mode)) {
        errno = EINVAL;
        return (void *)0;
    }
    f = mlib_file_allocate();
    if (!f) return (void *)0;
    mlib_store_state_pointer(&f->_object_root, (mlib_state_pointer)cookie);
    mlib_cookie_functions(f)->read = functions.read;
    mlib_cookie_functions(f)->write = functions.write;
    mlib_cookie_functions(f)->seek = functions.seek;
    mlib_cookie_functions(f)->close = functions.close;

    raw = mlib_raw(f);
    memset(raw, 0, sizeof(*raw));
    if (!strchr(mode, '+')) raw->flags = (*mode == 'r') ? F_NOWR : F_NORD;
    raw->fd = -1;
    raw->buf = (unsigned char *)(void *)f->_buffer_root + UNGET;
    raw->buf_size = BUFSIZ;
    raw->lbf = EOF;
    raw->read = mlib_cookie_read;
    raw->write = mlib_cookie_write;
    raw->seek = mlib_cookie_seek;
    raw->close = mlib_cookie_close;
    raw->lock = -1;

    f->_active = 1;
    mlib_ofl_add(f);
    return f;
}

c2go_extern int mlib_fwide(mlib_FILE *stream, int mode)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return 0;
    result = __c2go_file_raw_fwide(raw, mode);
    mlib_file_release(f);
    return result;
}

c2go_extern wint_t mlib_fputwc(wchar_t character, mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    wint_t result;
    if (!raw) return WEOF;
    result = __c2go_file_raw_fputwc(character, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern wint_t mlib_putwc(wchar_t character, mlib_FILE *stream)
{
    return mlib_fputwc(character, stream);
}

c2go_extern wint_t mlib_putwchar(wchar_t character)
{
    return mlib_fputwc(character, mlib_stdfile(1));
}

c2go_extern wint_t mlib_fgetwc(mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    wint_t result;
    if (!raw) return WEOF;
    result = __c2go_file_raw_fgetwc(raw);
    mlib_file_release(f);
    return result;
}

c2go_extern wint_t mlib_getwc(mlib_FILE *stream)
{
    return mlib_fgetwc(stream);
}

c2go_extern wint_t mlib_getwchar(void)
{
    return mlib_fgetwc(mlib_stdfile(0));
}

c2go_extern int mlib_fputws(const wchar_t *restrict text,
                            mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = __c2go_file_raw_fputws(text, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern wchar_t *mlib_fgetws(wchar_t *restrict text, int count,
                                  mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    wchar_t *result;
    if (!raw) return (void *)0;
    result = __c2go_file_raw_fgetws(text, count, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern wint_t mlib_ungetwc(wint_t character, mlib_FILE *stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    wint_t result;
    if (!raw) return WEOF;
    result = __c2go_file_raw_ungetwc(character, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_vfwprintf(mlib_FILE *restrict stream,
                               const wchar_t *restrict format,
                               va_list arguments)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    int result;
    if (!raw) return -1;
    result = __c2go_file_raw_vfwprintf(raw, format, arguments);
    mlib_file_release(f);
    return result;
}

c2go_extern int mlib_fwprintf(mlib_FILE *restrict stream,
                              const wchar_t *restrict format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = mlib_vfwprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int mlib_vwprintf(const wchar_t *restrict format,
                              va_list arguments)
{
    return mlib_vfwprintf(mlib_stdfile(1), format, arguments);
}

c2go_extern int mlib_wprintf(const wchar_t *restrict format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = mlib_vwprintf(format, arguments);
    va_end(arguments);
    return result;
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
    memset(f->_cookie_raw, 0, sizeof(f->_cookie_raw));
    mlib_clear_state_pointer(&f->_object_root);
    mlib_clear_state_pointer(&f->_result_pointer_slot);
    mlib_clear_state_pointer(&f->_result_size_slot);
    mlib_clear_buffer_pointer(&f->_buffer_root);
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

c2go_extern ssize_t mlib_getdelim(char **restrict line,
                                   size_t *restrict capacity, int delimiter,
                                   mlib_FILE *restrict stream)
{
    mlib_file_pointer f = stream;
    FILE *raw = mlib_file_acquire(f);
    ssize_t result;
    if (!raw) return -1;
    result = __c2go_file_raw_getdelim_managed((mlib_line_slot)line,
        (mlib_size_slot)capacity, delimiter, raw);
    mlib_file_release(f);
    return result;
}

c2go_extern ssize_t mlib_getline(char **restrict line,
                                  size_t *restrict capacity,
                                  mlib_FILE *restrict stream)
{
    return mlib_getdelim(line, capacity, '\n', stream);
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
