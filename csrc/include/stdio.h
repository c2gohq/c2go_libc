/* stdio.h — standard I/O. FILE is opaque here; its full layout is in the
 * internal <bits/stdio_impl.h>. A FILE and its buffer are PLAIN (unmanaged)
 * libc memory — mirroring musl — because a FILE holds only bytes, an int fd,
 * and function pointers (uintptr in c2go), never a GC-managed pointer, so
 * there is no GC-invisible-root hazard and no need for gc_malloc. */
#ifndef _STDIO_H
#define _STDIO_H

#define __NEED_FILE
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_va_list
#include <bits/alltypes.h>
#include <c2go.h>       /* c2go_linkname + C2GO_GOABI0 for the exported surface */

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

/* c2go's own stdio uses these buffer / limit hints internally (musl values). */
#define BUFSIZ       1024
#define FILENAME_MAX 4096
#define FOPEN_MAX    1000
#define TMP_MAX      10000
#define L_tmpnam     20

typedef long long fpos_t;   /* 64-bit stream position */

/* Std stream access goes through a tiny GoABI0 accessor: the three FILE
 * pointers are libc-owned DATA globals (source/stdio.c), and a cross-PACKAGE
 * data reference has no clean mechanism (a bare extern emits a package-local
 * symbol that fails to link; a variable c2go_linkname breaks the defining
 * package's own emission). Functions cross packages cleanly, so the public
 * spelling is a call — source/stdio.c #undefs these and uses the variables
 * directly. */
FILE *__c2go_stdfile(int which)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_stdfile", C2GO_GOABI0);
#define stdin  (__c2go_stdfile(0))
#define stdout (__c2go_stdfile(1))
#define stderr (__c2go_stdfile(2))

/* open / close */
FILE *fopen(const char *__restrict, const char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fopen", C2GO_GOABI0);
FILE *freopen(const char *__restrict, const char *__restrict, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.freopen", C2GO_GOABI0);
FILE *fdopen(int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fdopen", C2GO_GOABI0);
int   fclose(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fclose", C2GO_GOABI0);
int   remove(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.remove", C2GO_GOABI0);
/* rename is a package-provided C wrapper on both OSes, reached through the same
 * lowercase symbol: source/io_posix.c over a Go syscall shim on Unix; on Windows
 * source/io_windows.c over a Go shim too (MoveFileEx, for POSIX replace
 * semantics). Both keep the binding dl-free. */
int   rename(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rename", C2GO_GOABI0);

/* status */
int  feof(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.feof", C2GO_GOABI0);
int  ferror(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ferror", C2GO_GOABI0);
void clearerr(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.clearerr", C2GO_GOABI0);
int  fflush(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fflush", C2GO_GOABI0);

/* positioning */
int   fseek(FILE *, long, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fseek", C2GO_GOABI0);
long  ftell(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ftell", C2GO_GOABI0);
void  rewind(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rewind", C2GO_GOABI0);
int   fgetpos(FILE *__restrict, fpos_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fgetpos", C2GO_GOABI0);
int   fsetpos(FILE *, const fpos_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fsetpos", C2GO_GOABI0);
int   fseeko(FILE *, off_t, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fseeko", C2GO_GOABI0);
off_t ftello(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ftello", C2GO_GOABI0);

/* block I/O */
size_t fread(void *__restrict, size_t, size_t, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fread", C2GO_GOABI0);
size_t fwrite(const void *__restrict, size_t, size_t, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fwrite", C2GO_GOABI0);

/* character I/O */
int   fgetc(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fgetc", C2GO_GOABI0);
int   getc(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getc", C2GO_GOABI0);
int   getchar(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.getchar", C2GO_GOABI0);
int   ungetc(int, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ungetc", C2GO_GOABI0);
int   fputc(int, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fputc", C2GO_GOABI0);
int   putc(int, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.putc", C2GO_GOABI0);
int   putchar(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.putchar", C2GO_GOABI0);
char *fgets(char *__restrict, int, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fgets", C2GO_GOABI0);
int   fputs(const char *__restrict, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fputs", C2GO_GOABI0);
int   puts(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.puts", C2GO_GOABI0);
ssize_t getline(char **__restrict, size_t *__restrict, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.getline", C2GO_GOABI0);
ssize_t getdelim(char **__restrict, size_t *__restrict, int, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.getdelim", C2GO_GOABI0);

/* formatted output */
int printf(const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.printf", C2GO_GOABI0);
int fprintf(FILE *__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.fprintf", C2GO_GOABI0);
int sprintf(char *__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.sprintf", C2GO_GOABI0);
int snprintf(char *__restrict, size_t, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.snprintf", C2GO_GOABI0);
int vprintf(const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vprintf", C2GO_GOABI0);
int vfprintf(FILE *__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vfprintf", C2GO_GOABI0);
int vsprintf(char *__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vsprintf", C2GO_GOABI0);
int vsnprintf(char *__restrict, size_t, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vsnprintf", C2GO_GOABI0);
/* POSIX/GNU formatted-output extensions (source/stdio.c) */
int dprintf(int, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.dprintf", C2GO_GOABI0);
int vdprintf(int, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vdprintf", C2GO_GOABI0);
int asprintf(char **__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.asprintf", C2GO_GOABI0);
int vasprintf(char **__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vasprintf", C2GO_GOABI0);

/* formatted input */
int scanf(const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.scanf", C2GO_GOABI0);
int fscanf(FILE *__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.fscanf", C2GO_GOABI0);
int sscanf(const char *__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.sscanf", C2GO_GOABI0);
int vscanf(const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vscanf", C2GO_GOABI0);
int vfscanf(FILE *__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vfscanf", C2GO_GOABI0);
int vsscanf(const char *__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vsscanf", C2GO_GOABI0);

/* misc */
int   fileno(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fileno", C2GO_GOABI0);
void  perror(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.perror", C2GO_GOABI0);
int   setvbuf(FILE *__restrict, char *__restrict, int, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.setvbuf", C2GO_GOABI0);
void  setbuf(FILE *__restrict, char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.setbuf", C2GO_GOABI0);
/* BSD setvbuf shorthands + legacy word I/O (musl one-liners, source/stdio.c) */
void  setbuffer(FILE *, char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.setbuffer", C2GO_GOABI0);
void  setlinebuf(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.setlinebuf", C2GO_GOABI0);
int   getw(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getw", C2GO_GOABI0);
int   putw(int, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.putw", C2GO_GOABI0);
/* tmpnam is intentionally absent: obsolescent (its returned name races between
 * check and use) and unimplemented here; mkstemp (stdlib.h) is the safe
 * replacement. The L_tmpnam / TMP_MAX macros above stay (standard stdio.h). */
/* tmpfile: a delete-on-close temp stream (source/stdio.c). Unix builds a name
 * under /tmp, opens O_EXCL and immediately unlinks it (anonymous until closed).
 * Windows cannot unlink an open file, so it opens with the CRT's _O_TEMPORARY
 * (delete when the last fd closes) in os.TempDir(); either way fclose removes it.
 * Both keep musl's retry loop / name build / __fdopen in C, borrowing only the
 * temp dir + random word from Go. */
FILE *tmpfile(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.tmpfile", C2GO_GOABI0);

/* fmemopen / open_memstream / fopencookie (source/stdio.c). These back a FILE
 * with a caller buffer, a dynamically grown buffer, or caller-supplied I/O
 * callbacks. The old "GC-invisible root" reason for omitting them is obsolete:
 * since malloc is a Go make() + handle-table root (../malloc.go), a memstream's
 * OWN buffers are rooted exactly like every other FILE's (see the top comment).
 * Two caller-side rules remain — the same ones that apply to any libc call that
 * takes a pointer or a callback:
 *   - a Go-backed pointer handed in (fmemopen's buf, fopencookie's cookie) must
 *     be kept alive by the caller for the stream's lifetime; the noscan FILE
 *     does not root it (identical to fread(buf,...) keepalive, just longer-lived);
 *   - fopencookie's io callbacks must be c2go-compiled C functions — they are
 *     called through c2go's internal ABI, exactly like the FILE's own read/write
 *     slots (and like a qsort comparator). A host libc function pointer is not
 *     callable as a cookie op until the callout ABI bridge exists. */
typedef ssize_t (*cookie_read_function_t)(void *, char *, size_t);
typedef ssize_t (*cookie_write_function_t)(void *, const char *, size_t);
typedef int     (*cookie_seek_function_t)(void *, off_t *, int);
typedef int     (*cookie_close_function_t)(void *);
typedef struct {
	cookie_read_function_t  read;
	cookie_write_function_t write;
	cookie_seek_function_t  seek;
	cookie_close_function_t close;
} cookie_io_functions_t;

FILE *fmemopen(void *__restrict, size_t, const char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmemopen", C2GO_GOABI0);
FILE *open_memstream(char **, size_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.open_memstream", C2GO_GOABI0);
FILE *fopencookie(void *, const char *, cookie_io_functions_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.fopencookie", C2GO_GOABI0);

/* gets / mktemp / tmpnam / tempnam stay omitted: each has no bounded-safe form
 * (gets can't cap input; mktemp/tmpnam/tempnam return a name that races between
 * the check and the open — TOCTOU). fgets and mkstemp/mkdtemp are the safe
 * replacements. This is a decision, not a coverage gap. */

/* popen/pclose (#662, cross-platform since #678 — MinGW has _popen + the
 * OLDNAMES alias): spawn via the Go bridge (os/exec, as system()), stream
 * via fdopen. pclose returns the raw wait status on unix (decode with the
 * <sys/wait.h> macros) and the command's EXIT CODE on windows (the _pclose
 * shape; sys/wait.h stays #error-guarded there). */
FILE *popen(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.popen", C2GO_GOABI0);
int pclose(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pclose", C2GO_GOABI0);

/* _unlocked quartet (#654): POSIX allows these to BE the locked forms — the
 * per-FILE lock is goid-recursive (#664), so calling them under flockfile
 * composes; the "no locking" latitude is an optimization permit, not a
 * requirement. Lua's l_getc (LUA_USE_POSIX) lands here. */
#define getc_unlocked(f)     getc(f)
#define getchar_unlocked()   getchar()
#define putc_unlocked(c, f)  putc((c), (f))
#define putchar_unlocked(c)  putchar(c)

/* flockfile family (#664): goid-recursive per-FILE lock (source/stdio.c). */
void flockfile(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.flockfile", C2GO_GOABI0);
int ftrylockfile(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ftrylockfile", C2GO_GOABI0);
void funlockfile(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.funlockfile", C2GO_GOABI0);

#endif /* _STDIO_H */
