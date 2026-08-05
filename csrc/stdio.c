/* stdio.c — buffered formatted I/O, ported from musl's stdio. Covers:
 *   - printf family (printf/fprintf/sprintf/snprintf + v* variants), incl.
 *     %f/%e/%g/%a via fmt_fp;
 *   - scanf family (scanf/fscanf/sscanf + v* variants) via vfscanf/__intscan/
 *     __shgetc (#583: %d/%i/%o/%u/%x/%p, %s/%c/%[, %n) and __floatscan
 *     (#589: %a/%e/%f/%g, long double taken as double like fmt_fp);
 *   - the stdout/stderr/stdin FILE objects + the FILE write (__fwritex/
 *     __stdio_write) and read (__stdio_read/__toread/__uflow) paths;
 *   - fopen/fdopen/fclose (#594) + the open-file list backing fflush(NULL);
 *     freopen is a follow-up (needs dup2/fcntl syscall wrappers).
 * The printf wide-char conversions (%lc/%ls/%C/%S) encode wchar_t via
 * wctomb/wctomb_pair; on Windows (wchar_t=uint16) a supplementary scalar is
 * carried as a UTF-16 surrogate PAIR (combined through __surrogate_to_utf8).
 *
 * Everything internal is `static` (one TU, no cross-TU helpers). The PUBLIC
 * surface is marked c2go_extern (owns the ABI0 libc symbol); the matching
 * <stdio.h> declarations carry c2go_linkname(..., C2GO_GOABI0).
 *
 * Adaptations from musl (faithful otherwise — the printf_core formatter is
 * musl's, unchanged for the integer/string cases):
 *   - Locking: musl's thread-owned recursive flockfile is represented by one
 *     goroutine-owned recursive Go lock per FILE, bridged through
 *     _c2go_file_lock/_unlock (stdio.go, #659/#664). The explicit unlocked
 *     inner primitives keep compound operations within one clear lock scope.
 *   - Write callback: c2go has no writev, so __stdio_write emits its two
 *     segments (the buffered [wbase,wpos) then the caller's [buf,buf+len)) with
 *     write() calls, preserving musl's partial-write + error return semantics.
 *   - __stdout_write: no ioctl/TIOCGWINSZ, so no tty line-buffer detection —
 *     stdout stays fully buffered.
 *   - fflush(NULL): no open-file list in Phase 1 (no fopen), so it flushes only
 *     stdout + stderr.
 */
#include <stdio.h>
#include <bits/stdio_impl.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> /* mkdir for mkdtemp (cross-platform stat layer) */
#include <wchar.h>    /* wchar_t + wcstod/wcstol family c2go_linkname decls */
#include <bits/wchar_impl.h> /* WCHAR_UTF16 / SURR_* — wide FILE I/O surrogate pairing */
#include <wctype.h>   /* iswspace (leading-whitespace skip in the wide scans) */
#include <inttypes.h> /* wcstoimax / wcstoumax c2go_linkname decls */
#include <c2go.h>

/* This file DEFINES the std streams: bypass <stdio.h>'s accessor macros (the
 * cross-package spelling) and use the variables directly. */
#undef stdin
#undef stdout
#undef stderr
extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

/* stdout/stderr/stdin are #define'd to (stdin) etc. by <stdio.h>; undo that so
 * the definitions below (and internal references) name the real objects. */
#undef stdin
#undef stdout
#undef stderr

/* Positional-argument ceiling (musl's value); sizes the nl_arg/nl_type scratch
 * arrays used by the %n$ machinery. */
#define NL_ARGMAX 9

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* ── Locking (per-FILE Go mutex, #659; see stdio.go) ─────────────────────── */


/* Open-file list lock — a SECOND global mutex, distinct from the stdio lock, so
 * fflush(NULL) can hold the ofl lock while it re-locks each file's stdio lock
 * (musl relies on the list and stream locks being different). See stdio.go. */
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_ofl_lock", C2GO_GOABI0)
void _c2go_ofl_lock(void);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_ofl_unlock", C2GO_GOABI0)
void _c2go_ofl_unlock(void);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_file_lock", C2GO_GOABI0)
void _c2go_file_lock(unsigned long long *idp);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_file_unlock", C2GO_GOABI0)
void _c2go_file_unlock(unsigned long long *idp);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_file_lock_drop", C2GO_GOABI0)
void _c2go_file_lock_drop(unsigned long long *idp);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_file_trylock", C2GO_GOABI0)
int _c2go_file_trylock(unsigned long long *idp);

/* Per-FILE locking (#659, decision ②, musl parity): each stream owns its own
 * mutex (Go-side, rooted by f->lockid), so one goroutine's blocking
 * read(stdin) no longer freezes every other stream — the old process-global
 * stdio mutex did. The shared Go lock is goroutine-recursive, matching musl's
 * thread-recursive behavior. */
static int __lockfile(FILE *f) { _c2go_file_lock(&f->lockid); return 1; }
static void __unlockfile(FILE *f) { _c2go_file_unlock(&f->lockid); }

/* flockfile family (#664, musl stdio/flockfile.c semantics): the per-FILE
 * lock is goid-RECURSIVE (stdio.go), so a flockfile'd region may call any
 * stdio function on f — internal FLOCKs nest. A stack FILE (lock < 0) is not
 * a lockable stream; the calls are no-ops there, as for FLOCK. */
c2go_extern void flockfile(FILE *f)
{
	if (f->lock >= 0)
		_c2go_file_lock(&f->lockid);
}

c2go_extern int ftrylockfile(FILE *f)
{
	if (f->lock < 0)
		return 0;
	return _c2go_file_trylock(&f->lockid);
}

c2go_extern void funlockfile(FILE *f)
{
	if (f->lock >= 0)
		_c2go_file_unlock(&f->lockid);
}

/* musl's FLOCK/FUNLOCK, verbatim (stdio_impl.h). A stream with lock<0 (the
 * stack FILE in vsnprintf) is never locked. */
#define FFINALLOCK(f) ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FLOCK(f) int __need_unlock = ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FUNLOCK(f) do { if (__need_unlock) __unlockfile((f)); } while (0)

/* Open-file list — defined with the fopen family below; fflush(NULL) walks it. */
static FILE **__ofl_lock(void);
static void __ofl_unlock(void);

/* musl's flag-test + putc_unlocked macros (stdio_impl.h). The public
 * <stdio.h> now also defines the _unlocked quartet (#654) — drop those here
 * so this TU keeps its internal fast forms. */
#undef getc_unlocked
#undef getchar_unlocked
#undef putc_unlocked
#undef putchar_unlocked
#define ferror(f) ((f)->flags & F_ERR)
#define feof(f)   ((f)->flags & F_EOF)

#define putc_unlocked(c, f) \
	( (((unsigned char)(c)!=(f)->lbf && (f)->wpos!=(f)->wend)) \
	? *(f)->wpos++ = (unsigned char)(c) \
	: __overflow((f),(unsigned char)(c)) )

/* ── FILE write path ────────────────────────────────────────────────────── */

/* musl __towrite.c (dropped the __stdio_exit hook — no atexit flushing here). */
static int __towrite(FILE *f)
{
	f->mode |= f->mode-1;
	if (f->flags & F_NOWR) {
		f->flags |= F_ERR;
		return EOF;
	}
	/* Clear read buffer (easier than summoning nasal demons) */
	f->rpos = f->rend = 0;

	/* Activate write through the buffer. */
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;

	return 0;
}

/* musl __stdio_write.c, writev -> write loop. Emits [wbase,wpos) then
 * [buf,buf+len), handles partial writes, and preserves musl's return value:
 * on full success reset wend/wpos/wbase to buf and return len; on error set
 * wpos=wbase=wend=0, flags|=F_ERR, and return the count of `buf` already out. */
static size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	unsigned char *b0 = f->wbase;
	size_t l0 = f->wpos - f->wbase;   /* buffered segment */
	const unsigned char *b1 = buf;
	size_t l1 = len;                  /* caller's segment */

	while (l0) {
		ssize_t cnt = write(f->fd, b0, l0);
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			return 0;   /* nothing of `buf` was written */
		}
		b0 += cnt;
		l0 -= (size_t)cnt;
	}
	while (l1) {
		ssize_t cnt = write(f->fd, b1, l1);
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			return len - l1;   /* partial count of `buf` written */
		}
		b1 += cnt;
		l1 -= (size_t)cnt;
	}
	f->wend = f->buf + f->buf_size;
	f->wpos = f->wbase = f->buf;
	return len;
}

/* musl __stdio_seek.c / __stdio_close.c, minimal (no __aio_close). */
static off_t __stdio_seek(FILE *f, off_t off, int whence)
{
	return lseek(f->fd, off, whence);
}

static int __stdio_close(FILE *f)
{
	return close(f->fd);
}

/* ── Std streams: VIRTUALIZED descriptors 0/1/2 ────────────────────────────
 * In the C world, fd numbers 0/1/2 denote Go's LIVE os.Stdin/os.Stdout/
 * os.Stderr — the fd LAYER itself routes them (unix: the __c2go_syscall_*
 * shims in ../io.go & friends; Windows: source/io_windows.c), so read(0),
 * write(1), lseek, isatty, fstat, dup, fdopen(1) and the std FILE streams all
 * resolve to the same live sink, and fileno(stdout) is literally 1 — POSIX
 * descriptor identity holds inside the C world while the os package variables
 * remain the single source of truth (no libc-side cache; a Go-side
 * os.Stdout = f reassignment is visible to the very next C operation).
 * C-side redirects (freopen/dup2 onto 0/1/2) stay kernel-level and are
 * identity-gated (see freopen below).
 *
 * The std FILEs therefore use the ORDINARY musl callbacks for read/seek
 * (their fd-layer calls route), plus two specials:
 *   - __std_go_write: one Go call per logical flush (__c2go_std_writev)
 *     carrying BOTH musl segments, so the live os.Std* is snapshotted once
 *     per flush and a single fwrite cannot split across two sinks.
 *   - __std_go_close: fclose(std) must not close the Go-owned kernel fd. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_writev", C2GO_GOABI0)
size_t __c2go_std_writev(int which, const void *b0, size_t l0,
                         const void *b1, size_t l1, int *eout);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_isdefault", C2GO_GOABI0)
int __c2go_std_isdefault(int which);

/* __std_go_write: __stdio_write's exact contract over one __c2go_std_writev
 * call — on full success reset wend/wpos/wbase and return len; on error set
 * errno (the shim's out-param), wpos=wbase=wend=0, F_ERR, and return the
 * count of `buf` already written (0 if the buffered segment failed). */
static size_t __std_go_write(FILE *f, const unsigned char *buf, size_t len)
{
	int e = 0;
	size_t k = __c2go_std_writev(f->fd, f->wbase, f->wpos - f->wbase,
	                             buf, len, &e);
	if (e) {
		errno = e;
		f->wpos = f->wbase = f->wend = 0;
		f->flags |= F_ERR;
		return k;
	}
	f->wend = f->buf + f->buf_size;
	f->wpos = f->wbase = f->buf;
	return len;
}

/* fds 0/1/2 belong to Go's os package: fclose(std) flushes and kills the
 * stream but must NOT close the kernel fd out from under os.Std*. */
static int __std_go_close(FILE *f)
{
	(void)f;
	return 0;
}

/* musl __stdout_write.c: stdout defaults to line-buffered (.lbf='\n') so an
 * interactive prompt reaches the terminal before a blocking read; a pipe/file is
 * dropped to fully buffered on the first write. musl probes the tty with
 * ioctl(TIOCGWINSZ); c2go has no ioctl, so it uses isatty(f->fd) — fd 1 is the
 * virtualized descriptor, and the fd-layer isatty is identity-gated (a host-
 * swapped os.Stdout is reported "not a tty", correct for the pipe/file swaps
 * that occur in practice). F_SVB means the user already fixed the mode via
 * setvbuf — leave it. On Windows there is no isatty, so stdout stays fully
 * buffered (the exit-time flush persists it anyway). The write callback swaps
 * to plain __std_go_write, so this runs once. */
static size_t __stdout_write(FILE *f, const unsigned char *buf, size_t len)
{
	f->write = __std_go_write;
#if !defined(_WIN32)
	if (!(f->flags & F_SVB) && !isatty(f->fd)) f->lbf = -1;
#else
	if (!(f->flags & F_SVB)) f->lbf = -1;
#endif
	return __std_go_write(f, buf, len);
}

/* musl __overflow.c (needed by putc_unlocked / fputc). */
static int __overflow(FILE *f, int _c)
{
	unsigned char c = _c;
	if (!f->wend && __towrite(f)) return EOF;
	if (f->wpos != f->wend && c != f->lbf) return *f->wpos++ = c;
	if (f->write(f, &c, 1) != 1) return EOF;
	return c;
}

/* musl __fwritex (fwrite.c): the buffered write core. */
static size_t __fwritex(const unsigned char *restrict s, size_t l, FILE *restrict f)
{
	size_t i=0;

	if (!f->wend && __towrite(f)) return 0;

	if (l > (size_t)(f->wend - f->wpos)) return f->write(f, s, l);

	if (f->lbf >= 0) {
		/* Match /^(.*\n|)/ */
		for (i=l; i && s[i-1] != '\n'; i--);
		if (i) {
			size_t n = f->write(f, s, i);
			if (n < i) return n;
			s += i;
			l -= i;
		}
	}

	memcpy(f->wpos, s, l);
	f->wpos += l;
	return l+i;
}

/* ── FILE read path (#583, for scanf/getc) ──────────────────────────────── */

/* musl __stdio_read.c, de-readv'd. musl issues one readv that fills the
 * caller's buffer AND reads ahead into f->buf; c2go has no readv, so:
 *   - len>1 or unbuffered: read straight into the caller's buffer (the fread
 *     path). The readahead into f->buf is dropped — correctness is unchanged,
 *     only an extra syscall per refill for large fread.
 *   - len==1, buffered (the getc/scanf path via __uflow): refill f->buf, hand
 *     back one byte, and leave the rest buffered in [rpos,rend) so subsequent
 *     shgetc reads take no syscall.
 * On EOF/error, mirror musl: set F_EOF or F_ERR and return 0. */
static size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
	ssize_t cnt;
	if (len > 1 || !f->buf_size) {
		cnt = read(f->fd, buf, len);
		if (cnt <= 0) {
			f->flags |= cnt ? F_ERR : F_EOF;
			return 0;
		}
		return cnt;
	}
	cnt = read(f->fd, f->buf, f->buf_size);
	if (cnt <= 0) {
		f->flags |= cnt ? F_ERR : F_EOF;
		return 0;
	}
	f->rpos = f->buf;
	f->rend = f->buf + cnt;
	buf[len-1] = *f->rpos++;
	return len;
}

/* ── Standard streams ───────────────────────────────────────────────────── */
/* NOTE: these static FILE initializers hold function-POINTER fields
 * (.write/.seek/.close). If c2go's codegen cannot lower a function-pointer
 * global initializer this is where it surfaces (a real finding, per the task).
 *
 * .lock = 0 (not musl's -1): with lock>=0, FLOCK engages the stream's own lock,
 * serialising concurrent goroutine access to the shared streams. */

static unsigned char __stdout_buf[BUFSIZ+UNGET];
static FILE __stdout_FILE = {
	.buf = __stdout_buf+UNGET,
	.buf_size = sizeof __stdout_buf - UNGET,
	.fd = 1,                   /* virtualized descriptor: the fd layer resolves it to the live os.Stdout */
	.flags = F_PERM | F_NORD,
	.lbf = '\n',               /* line-buffered by default; __stdout_write drops to full for a non-tty */
	.write = __stdout_write,
	.seek = __stdio_seek,
	.close = __std_go_close,
	.lock = 0,
};
FILE *const stdout = &__stdout_FILE;

/* stderr is unbuffered (buf_size 0): __fwritex sends every write straight
 * through f->write. */
static unsigned char __stderr_buf[UNGET];
static FILE __stderr_FILE = {
	.buf = __stderr_buf+UNGET,
	.buf_size = 0,
	.fd = 2,                   /* virtualized descriptor (live os.Stderr) */
	.flags = F_PERM | F_NORD,
	.lbf = -1,
	.write = __std_go_write,
	.seek = __stdio_seek,
	.close = __std_go_close,
	.lock = 0,
};
FILE *const stderr = &__stderr_FILE;

/* stdin: read path wired in #583 (scanf). The UNGET-byte headroom before
 * __stdin_buf lets __shgetc push a byte back before the buffer start. */
static unsigned char __stdin_buf[BUFSIZ+UNGET];
static FILE __stdin_FILE = {
	.buf = __stdin_buf+UNGET,
	.buf_size = sizeof __stdin_buf - UNGET,
	.fd = 0,                   /* virtualized descriptor (live os.Stdin) */
	.flags = F_PERM | F_NOWR,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.close = __std_go_close,
	.lock = 0,
};
FILE *const stdin = &__stdin_FILE;

/* __c2go_stdfile: the cross-package std stream accessor behind <stdio.h>'s
 * stdin/stdout/stderr macros. A DATA global cannot be referenced cleanly from
 * another c2go package (a bare extern emits a package-local symbol), but a
 * GoABI0 function call crosses packages like any libc function. */
c2go_extern FILE *__c2go_stdfile(int which)
{
	switch (which) {
	case 0:  return stdin;
	case 1:  return stdout;
	default: return stderr;
	}
}

/* ── fwrite ─────────────────────────────────────────────────────────────── */

c2go_extern
size_t fwrite(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t k, l = size*nmemb;
	if (!size) nmemb = 0;
	FLOCK(f);
	k = __fwritex(src, l, f);
	FUNLOCK(f);
	return k==l ? nmemb : k/size;
}

/* ── printf_core (musl vfprintf.c) ──────────────────────────────────────── */

/* Convenient bit representation for modifier flags. */
#define ALT_FORM   (1U<<'#'-' ')
#define ZERO_PAD   (1U<<'0'-' ')
#define LEFT_ADJ   (1U<<'-'-' ')
#define PAD_POS    (1U<<' '-' ')
#define MARK_POS   (1U<<'+'-' ')
#define GROUPED    (1U<<'\''-' ')

#define FLAGMASK (ALT_FORM|ZERO_PAD|LEFT_ADJ|PAD_POS|MARK_POS|GROUPED)

enum {
	BARE, LPRE, LLPRE, HPRE, HHPRE, BIGLPRE,
	ZTPRE, JPRE,
	STOP,
	PTR, INT, UINT, ULLONG,
	LONG, ULONG,
	SHORT, USHORT, CHAR, UCHAR,
	LLONG, SIZET, IMAX, UMAX, PDIFF, UIPTR,
	DBL, LDBL,
	NOARG,
	MAXSTATE
};

#define S(x) [(x)-'A']

static const unsigned char states[]['z'-'A'+1] = {
	{ /* 0: bare types */
		S('d') = INT, S('i') = INT,
		S('o') = UINT, S('u') = UINT, S('x') = UINT, S('X') = UINT,
		S('e') = DBL, S('f') = DBL, S('g') = DBL, S('a') = DBL,
		S('E') = DBL, S('F') = DBL, S('G') = DBL, S('A') = DBL,
		S('c') = INT, S('C') = UINT,
		S('s') = PTR, S('S') = PTR, S('p') = UIPTR, S('n') = PTR,
		S('m') = NOARG,
		S('l') = LPRE, S('h') = HPRE, S('L') = BIGLPRE,
		S('z') = ZTPRE, S('j') = JPRE, S('t') = ZTPRE,
	}, { /* 1: l-prefixed */
		S('d') = LONG, S('i') = LONG,
		S('o') = ULONG, S('u') = ULONG, S('x') = ULONG, S('X') = ULONG,
		S('e') = DBL, S('f') = DBL, S('g') = DBL, S('a') = DBL,
		S('E') = DBL, S('F') = DBL, S('G') = DBL, S('A') = DBL,
		S('c') = UINT, S('s') = PTR, S('n') = PTR,
		S('l') = LLPRE,
	}, { /* 2: ll-prefixed */
		S('d') = LLONG, S('i') = LLONG,
		S('o') = ULLONG, S('u') = ULLONG,
		S('x') = ULLONG, S('X') = ULLONG,
		S('n') = PTR,
	}, { /* 3: h-prefixed */
		S('d') = SHORT, S('i') = SHORT,
		S('o') = USHORT, S('u') = USHORT,
		S('x') = USHORT, S('X') = USHORT,
		S('n') = PTR,
		S('h') = HHPRE,
	}, { /* 4: hh-prefixed */
		S('d') = CHAR, S('i') = CHAR,
		S('o') = UCHAR, S('u') = UCHAR,
		S('x') = UCHAR, S('X') = UCHAR,
		S('n') = PTR,
	}, { /* 5: L-prefixed */
		S('e') = LDBL, S('f') = LDBL, S('g') = LDBL, S('a') = LDBL,
		S('E') = LDBL, S('F') = LDBL, S('G') = LDBL, S('A') = LDBL,
		S('n') = PTR,
	}, { /* 6: z- or t-prefixed (assumed to be same size) */
		S('d') = PDIFF, S('i') = PDIFF,
		S('o') = SIZET, S('u') = SIZET,
		S('x') = SIZET, S('X') = SIZET,
		S('n') = PTR,
	}, { /* 7: j-prefixed */
		S('d') = IMAX, S('i') = IMAX,
		S('o') = UMAX, S('u') = UMAX,
		S('x') = UMAX, S('X') = UMAX,
		S('n') = PTR,
	}
};

#define OOB(x) ((unsigned)(x)-'A' > 'z'-'A')

union arg
{
	uintmax_t i;
	long double f;
	void *p;
};

static void pop_arg(union arg *arg, int type, va_list *ap)
{
	switch (type) {
	       case PTR:	arg->p = va_arg(*ap, void *);
	break; case INT:	arg->i = va_arg(*ap, int);
	break; case UINT:	arg->i = va_arg(*ap, unsigned int);
	break; case LONG:	arg->i = va_arg(*ap, long);
	break; case ULONG:	arg->i = va_arg(*ap, unsigned long);
	break; case ULLONG:	arg->i = va_arg(*ap, unsigned long long);
	break; case SHORT:	arg->i = (short)va_arg(*ap, int);
	break; case USHORT:	arg->i = (unsigned short)va_arg(*ap, int);
	break; case CHAR:	arg->i = (signed char)va_arg(*ap, int);
	break; case UCHAR:	arg->i = (unsigned char)va_arg(*ap, int);
	break; case LLONG:	arg->i = va_arg(*ap, long long);
	break; case SIZET:	arg->i = va_arg(*ap, size_t);
	break; case IMAX:	arg->i = va_arg(*ap, intmax_t);
	break; case UMAX:	arg->i = va_arg(*ap, uintmax_t);
	break; case PDIFF:	arg->i = va_arg(*ap, ptrdiff_t);
	break; case UIPTR:	arg->i = (uintptr_t)va_arg(*ap, void *);
	break; case DBL:	arg->f = va_arg(*ap, double);
	break; case LDBL:	arg->f = va_arg(*ap, long double);
	}
}

static void out(FILE *f, const char *s, size_t l)
{
	if (!ferror(f)) __fwritex((void *)s, l, f);
}

static void pad(FILE *f, char c, int w, int l, int fl)
{
	char pad[256];
	if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w) return;
	l = w - l;
	memset(pad, c, (size_t)l>sizeof pad ? sizeof pad : (size_t)l);
	for (; (size_t)l >= sizeof pad; l -= sizeof pad)
		out(f, pad, sizeof pad);
	out(f, pad, l);
}

static const char xdigits[16] = {
	"0123456789ABCDEF"
};

static char *fmt_x(uintmax_t x, char *s, int lower)
{
	for (; x; x>>=4) *--s = xdigits[(x&15)]|lower;
	return s;
}

static char *fmt_o(uintmax_t x, char *s)
{
	for (; x; x>>=3) *--s = '0' + (x&7);
	return s;
}

static char *fmt_u(uintmax_t x, char *s)
{
	unsigned long y;
	for (   ; x>ULONG_MAX; x/=10) *--s = '0' + x%10;
	for (y=x;       y>=10; y/=10) *--s = '0' + y%10;
	if (y) *--s = '0' + y;
	return s;
}

static int getint(char **s) {
	int i;
	for (i=0; isdigit(**s); (*s)++) {
		if (i > INT_MAX/10U || **s-'0' > INT_MAX-10*i) i = -1;
		else i = 10*i + (**s-'0');
	}
	return i;
}

/* --- printf float support (#582): fmt_fp ported from musl 1.2.6's
 * src/stdio/vfprintf.c, adapted to operate on `double` instead of
 * `long double`. c2go's long double is arch-divergent (arm64: 64-bit,
 * identical to double; x86-64: 80-bit x87, which the Go-ABI0 backend does not
 * support), so the formatter always works in double precision: %f/%e/%g/%a are
 * exact for double arguments, and %Lf is formatted as its double value (a
 * documented precision limitation; a long-double path is deferred). frexp and
 * scalbn are the only math dependencies; c2go-libc has no <math.h>
 * implementation yet, so they are provided here as file-local helpers, and the
 * needed <float.h> double constants are defined below. --- */
#define C2GO_DBL_MANT_DIG 53
#define C2GO_DBL_MAX_EXP  1024
#define C2GO_DBL_EPSILON  0x1p-52
/* fmt_fp's big[] digit buffer. musl sizes it as a VLA from the runtime
 * mantissa/exponent width; specialized to double these are compile-time
 * constants (c2go rejects VLAs). One mantissa slot for 29 bits left of the
 * radix point + one per 29-21=8 bits to the right + a zero slot; the exponent
 * expansion needs (max_exp+mant+28+8)/9 slots. */
#define C2GO_FMT_FP_MANT_SLOTS (1 + (C2GO_DBL_MANT_DIG-29+7)/8 + 1)
#define C2GO_FMT_FP_EXP_SLOTS  ((C2GO_DBL_MAX_EXP+C2GO_DBL_MANT_DIG+28+8)/9)

/* musl src/math/frexp.c (double). */
static double fp_frexp(double x, int *e)
{
	union { double d; uint64_t i; } y = { x };
	int ee = y.i>>52 & 0x7ff;

	if (!ee) {
		if (x) {
			x = fp_frexp(x*0x1p64, e);
			*e -= 64;
		} else *e = 0;
		return x;
	} else if (ee == 0x7ff) {
		return x;
	}

	*e = ee - 0x3fe;
	y.i &= 0x800fffffffffffffull;
	y.i |= 0x3fe0000000000000ull;
	return y.d;
}

/* musl src/math/scalbn.c (double); double_t narrowed to double. */
static double fp_scalbn(double x, int n)
{
	union {double f; uint64_t i;} u;
	double y = x;

	if (n > 1023) {
		y *= 0x1p1023;
		n -= 1023;
		if (n > 1023) {
			y *= 0x1p1023;
			n -= 1023;
			if (n > 1023)
				n = 1023;
		}
	} else if (n < -1022) {
		/* make sure final n < -53 to avoid double
		   rounding in the subnormal range */
		y *= 0x1p-1022 * 0x1p53;
		n += 1022 - 53;
		if (n < -1022) {
			y *= 0x1p-1022 * 0x1p53;
			n += 1022 - 53;
			if (n < -1022)
				n = -1022;
		}
	}
	u.i = (uint64_t)(0x3ff+n)<<52;
	x = y * u.f;
	return x;
}

/* musl src/math/fmod.c (double), verbatim. __floatscan (#589) needs fmod for
 * the rounding tail; __builtin_fmod would lower to an external `fmod` call the
 * libc does not provide, so it is ported here alongside fp_frexp/fp_scalbn. */
static double fp_fmod(double x, double y)
{
	union {double f; uint64_t i;} ux = {x}, uy = {y};
	int ex = ux.i>>52 & 0x7ff;
	int ey = uy.i>>52 & 0x7ff;
	int sx = ux.i>>63;
	uint64_t i;
	uint64_t uxi = ux.i;

	if (uy.i<<1 == 0 || __builtin_isnan(y) || ex == 0x7ff)
		return (x*y)/(x*y);
	if (uxi<<1 <= uy.i<<1) {
		if (uxi<<1 == uy.i<<1)
			return 0*x;
		return x;
	}

	/* normalize x and y */
	if (!ex) {
		for (i = uxi<<12; i>>63 == 0; ex--, i <<= 1);
		uxi <<= -ex + 1;
	} else {
		uxi &= -1ULL >> 12;
		uxi |= 1ULL << 52;
	}
	if (!ey) {
		for (i = uy.i<<12; i>>63 == 0; ey--, i <<= 1);
		uy.i <<= -ey + 1;
	} else {
		uy.i &= -1ULL >> 12;
		uy.i |= 1ULL << 52;
	}

	/* x mod y */
	for (; ex > ey; ex--) {
		i = uxi - uy.i;
		if (i >> 63 == 0) {
			if (i == 0)
				return 0*x;
			uxi = i;
		}
		uxi <<= 1;
	}
	i = uxi - uy.i;
	if (i >> 63 == 0) {
		if (i == 0)
			return 0*x;
		uxi = i;
	}
	for (; uxi>>52 == 0; uxi <<= 1, ex--);

	/* scale result */
	if (ex > 0) {
		uxi -= 1ULL << 52;
		uxi |= (uint64_t)ex << 52;
	} else {
		uxi >>= -ex + 1;
	}
	uxi |= (uint64_t)sx << 63;
	ux.i = uxi;
	return ux.f;
}

static int fmt_fp(FILE *f, double y, int w, int p, int fl, int t)
{
	int max_mant_slots = C2GO_FMT_FP_MANT_SLOTS;
	uint32_t big[C2GO_FMT_FP_MANT_SLOTS + C2GO_FMT_FP_EXP_SLOTS];
	uint32_t *a, *d, *r, *z;
	int e2=0, e, i, j, l;
	char buf[9+C2GO_DBL_MANT_DIG/4], *s;
	const char *prefix="-0X+0X 0X-0x+0x 0x";
	int pl;
	char ebuf0[3*sizeof(int)], *ebuf=&ebuf0[3*sizeof(int)], *estr;

	pl=1;
	if (__builtin_signbit(y)) {
		y=-y;
	} else if (fl & MARK_POS) {
		prefix+=3;
	} else if (fl & PAD_POS) {
		prefix+=6;
	} else prefix++, pl=0;

	if (!__builtin_isfinite(y)) {
		const char *ss = (t&32)?"inf":"INF";
		if (y!=y) ss=(t&32)?"nan":"NAN";
		pad(f, ' ', w, 3+pl, fl&~ZERO_PAD);
		out(f, prefix, pl);
		out(f, ss, 3);
		pad(f, ' ', w, 3+pl, fl^LEFT_ADJ);
		return MAX(w, 3+pl);
	}

	y = fp_frexp(y, &e2) * 2;
	if (y) e2--;

	if ((t|32)=='a') {
		if (t&32) prefix += 9;
		pl += 2;

		if (p>=0 && p<(C2GO_DBL_MANT_DIG-1+3)/4) {
			double round = fp_scalbn(1, C2GO_DBL_MANT_DIG-1-(p*4));
			if (*prefix=='-') {
				y=-y;
				y-=round;
				y+=round;
				y=-y;
			} else {
				y+=round;
				y-=round;
			}
		}

		estr=fmt_u(e2<0 ? -e2 : e2, ebuf);
		if (estr==ebuf) *--estr='0';
		*--estr = (e2<0 ? '-' : '+');
		*--estr = t+('p'-'a');

		s=buf;
		do {
			int x=y;
			*s++=xdigits[x]|(t&32);
			y=16*(y-x);
			if (s-buf==1 && (y||p>0||(fl&ALT_FORM))) *s++='.';
		} while (y);

		if (p > INT_MAX-2-(ebuf-estr)-pl)
			return -1;
		if (p && s-buf-2 < p)
			l = (p+2) + (ebuf-estr);
		else
			l = (s-buf) + (ebuf-estr);

		pad(f, ' ', w, pl+l, fl);
		out(f, prefix, pl);
		pad(f, '0', w, pl+l, fl^ZERO_PAD);
		out(f, buf, s-buf);
		pad(f, '0', l-(ebuf-estr)-(s-buf), 0, 0);
		out(f, estr, ebuf-estr);
		pad(f, ' ', w, pl+l, fl^LEFT_ADJ);
		return MAX(w, pl+l);
	}
	if (p<0) p=6;

	if (y) y *= 0x1p28, e2-=28;

	if (e2<0) a=r=z=big;
	else a=r=z=big+sizeof(big)/sizeof(*big) - max_mant_slots - 1;

	do {
		*z = y;
		y = 1000000000*(y-*z++);
	} while (y);

	while (e2>0) {
		uint32_t carry=0;
		int sh=MIN(29,e2);
		for (d=z-1; d>=a; d--) {
			uint64_t x = ((uint64_t)*d<<sh)+carry;
			*d = x % 1000000000;
			carry = x / 1000000000;
		}
		if (carry) *--a = carry;
		while (z>a && !z[-1]) z--;
		e2-=sh;
	}
	while (e2<0) {
		uint32_t carry=0, *b;
		int sh=MIN(9,-e2), need=1+(p+C2GO_DBL_MANT_DIG/3U+8)/9;
		for (d=a; d<z; d++) {
			uint32_t rm = *d & (1<<sh)-1;
			*d = (*d>>sh) + carry;
			carry = (1000000000>>sh) * rm;
		}
		if (!*a) a++;
		if (carry) *z++ = carry;
		/* Avoid (slow!) computation past requested precision */
		b = (t|32)=='f' ? r : a;
		if (z-b > need) z = b+need;
		e2+=sh;
	}

	if (a<z) for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
	else e=0;

	/* Perform rounding: j is precision after the radix (possibly neg) */
	j = p - ((t|32)!='f')*e - ((t|32)=='g' && p);
	if (j < 9*(z-r-1)) {
		uint32_t x;
		/* We avoid C's broken division of negative numbers */
		d = r + 1 + ((j+9*C2GO_DBL_MAX_EXP)/9 - C2GO_DBL_MAX_EXP);
		j += 9*C2GO_DBL_MAX_EXP;
		j %= 9;
		for (i=10, j++; j<9; i*=10, j++);
		x = *d % i;
		/* Are there any significant digits past j? */
		if (x || d+1!=z) {
			double round = 2/C2GO_DBL_EPSILON;
			double small;
			if ((*d/i & 1) || (i==1000000000 && d>a && (d[-1]&1)))
				round += 2;
			if (x<i/2) small=0x0.8p0;
			else if (x==i/2 && d+1==z) small=0x1.0p0;
			else small=0x1.8p0;
			if (pl && *prefix=='-') round*=-1, small*=-1;
			*d -= x;
			/* Decide whether to round by probing round+small */
			if (round+small != round) {
				*d = *d + i;
				while (*d > 999999999) {
					*d--=0;
					if (d<a) *--a=0;
					(*d)++;
				}
				for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
			}
		}
		if (z>d+1) z=d+1;
	}
	for (; z>a && !z[-1]; z--);

	if ((t|32)=='g') {
		if (!p) p++;
		if (p>e && e>=-4) {
			t--;
			p-=e+1;
		} else {
			t-=2;
			p--;
		}
		if (!(fl&ALT_FORM)) {
			/* Count trailing zeros in last place */
			if (z>a && z[-1]) for (i=10, j=0; z[-1]%i==0; i*=10, j++);
			else j=9;
			if ((t|32)=='f')
				p = MIN(p,MAX(0,9*(z-r-1)-j));
			else
				p = MIN(p,MAX(0,9*(z-r-1)+e-j));
		}
	}
	if (p > INT_MAX-1-(p || (fl&ALT_FORM)))
		return -1;
	l = 1 + p + (p || (fl&ALT_FORM));
	if ((t|32)=='f') {
		if (e > INT_MAX-l) return -1;
		if (e>0) l+=e;
	} else {
		estr=fmt_u(e<0 ? -e : e, ebuf);
		while(ebuf-estr<2) *--estr='0';
		*--estr = (e<0 ? '-' : '+');
		*--estr = t;
		if (ebuf-estr > INT_MAX-l) return -1;
		l += ebuf-estr;
	}

	if (l > INT_MAX-pl) return -1;
	pad(f, ' ', w, pl+l, fl);
	out(f, prefix, pl);
	pad(f, '0', w, pl+l, fl^ZERO_PAD);

	if ((t|32)=='f') {
		if (a>r) a=r;
		for (d=a; d<=r; d++) {
			char *ss = fmt_u(*d, buf+9);
			if (d!=a) while (ss>buf) *--ss='0';
			else if (ss==buf+9) *--ss='0';
			out(f, ss, buf+9-ss);
		}
		if (p || (fl&ALT_FORM)) out(f, ".", 1);
		for (; d<z && p>0; d++, p-=9) {
			char *ss = fmt_u(*d, buf+9);
			while (ss>buf) *--ss='0';
			out(f, ss, MIN(9,p));
		}
		pad(f, '0', p+9, 9, 0);
	} else {
		if (z<=a) z=a+1;
		for (d=a; d<z && p>=0; d++) {
			char *ss = fmt_u(*d, buf+9);
			if (ss==buf+9) *--ss='0';
			if (d!=a) while (ss>buf) *--ss='0';
			else {
				out(f, ss++, 1);
				if (p>0||(fl&ALT_FORM)) out(f, ".", 1);
			}
			out(f, ss, MIN(buf+9-ss, p));
			p -= buf+9-ss;
		}
		pad(f, '0', p+18, 18, 0);
		out(f, estr, ebuf-estr);
	}

	pad(f, ' ', w, pl+l, fl^LEFT_ADJ);

	return MAX(w, pl+l);
}

/* Encode the next wide char at *pws to UTF-8 in mb[MB_LEN_MAX], advancing *pws by
 * one code unit (a BMP wchar_t, or any scalar on the UTF-32 targets) or two (a
 * UTF-16 surrogate PAIR on Windows). Returns the byte count, or -1 (EILSEQ) for a
 * lone/misordered surrogate. On the unix targets WCHAR_UTF16 folds to 0 and this
 * is exactly `wctomb(mb, *ws++)`; the narrow %ls/%lc two-pass loops stay byte-
 * synchronized because identical input always yields the same length + advance. */
static int wctomb_pair(char *mb, const wchar_t **pws)
{
	const wchar_t *ws = *pws;
	if (WCHAR_UTF16 && IS_HIGH_SURR(*ws)) {
		if (!IS_LOW_SURR(ws[1])) { errno = EILSEQ; return -1; }
		*pws = ws + 2;
		return __surrogate_to_utf8(mb, ws[0], ws[1]);
	}
	*pws = ws + 1;
	return wctomb(mb, *ws);
}

static int printf_core(FILE *f, const char *fmt, va_list *ap, union arg *nl_arg, int *nl_type)
{
	char *a, *z, *s=(char *)fmt;
	unsigned l10n=0, fl;
	int w, p, xp;
	union arg arg;
	int argpos;
	unsigned st, ps;
	int cnt=0, l=0;
	size_t i;
	char buf[sizeof(uintmax_t)*3];
	const char *prefix;
	int t, pl;
	wchar_t wc[3], *ws;
	char mb[4];

	for (;;) {
		/* This error is only specified for snprintf, but since it's
		 * unspecified for other forms, do the same. Stop immediately
		 * on overflow; otherwise %n could produce wrong results. */
		if (l > INT_MAX - cnt) goto overflow;

		/* Update output count, end loop when fmt is exhausted */
		cnt += l;
		if (!*s) break;

		/* Handle literal text and %% format specifiers */
		for (a=s; *s && *s!='%'; s++);
		for (z=s; s[0]=='%' && s[1]=='%'; z++, s+=2);
		if (z-a > INT_MAX-cnt) goto overflow;
		l = z-a;
		if (f) out(f, a, l);
		if (l) continue;

		if (isdigit(s[1]) && s[2]=='$') {
			l10n=1;
			argpos = s[1]-'0';
			s+=3;
		} else {
			argpos = -1;
			s++;
		}

		/* Read modifier flags */
		for (fl=0; (unsigned)*s-' '<32 && (FLAGMASK&(1U<<*s-' ')); s++)
			fl |= 1U<<*s-' ';

		/* Read field width */
		if (*s=='*') {
			if (isdigit(s[1]) && s[2]=='$') {
				l10n=1;
				if (!f) nl_type[s[1]-'0'] = INT, w = 0;
				else w = nl_arg[s[1]-'0'].i;
				s+=3;
			} else if (!l10n) {
				w = f ? va_arg(*ap, int) : 0;
				s++;
			} else goto inval;
			if (w<0) fl|=LEFT_ADJ, w=-w;
		} else if ((w=getint(&s))<0) goto overflow;

		/* Read precision */
		if (*s=='.' && s[1]=='*') {
			if (isdigit(s[2]) && s[3]=='$') {
				if (!f) nl_type[s[2]-'0'] = INT, p = 0;
				else p = nl_arg[s[2]-'0'].i;
				s+=4;
			} else if (!l10n) {
				p = f ? va_arg(*ap, int) : 0;
				s+=2;
			} else goto inval;
			xp = (p>=0);
		} else if (*s=='.') {
			s++;
			p = getint(&s);
			xp = 1;
		} else {
			p = -1;
			xp = 0;
		}

		/* Format specifier state machine */
		st=0;
		do {
			if (OOB(*s)) goto inval;
			ps=st;
			st=states[st]S(*s++);
		} while (st-1<STOP);
		if (!st) goto inval;

		/* Check validity of argument type (nl/normal) */
		if (st==NOARG) {
			if (argpos>=0) goto inval;
		} else {
			if (argpos>=0) {
				if (!f) nl_type[argpos]=st;
				else arg=nl_arg[argpos];
			} else if (f) pop_arg(&arg, st, ap);
			else return 0;
		}

		if (!f) continue;

		/* Do not process any new directives once in error state. */
		if (ferror(f)) return -1;

		z = buf + sizeof(buf);
		prefix = "-+   0X0x";
		pl = 0;
		t = s[-1];

		/* Transform ls,lc -> S,C */
		if (ps && (t&15)==3) t&=~32;

		/* - and 0 flags are mutually exclusive */
		if (fl & LEFT_ADJ) fl &= ~ZERO_PAD;

		switch(t) {
		case 'n':
			switch(ps) {
			case BARE: *(int *)arg.p = cnt; break;
			case LPRE: *(long *)arg.p = cnt; break;
			case LLPRE: *(long long *)arg.p = cnt; break;
			case HPRE: *(unsigned short *)arg.p = cnt; break;
			case HHPRE: *(unsigned char *)arg.p = cnt; break;
			case ZTPRE: *(size_t *)arg.p = cnt; break;
			case JPRE: *(uintmax_t *)arg.p = cnt; break;
			}
			continue;
		case 'p':
			p = MAX(p, 2*(int)sizeof(void*));
			t = 'x';
			fl |= ALT_FORM;
		case 'x': case 'X':
			a = fmt_x(arg.i, z, t&32);
			if (arg.i && (fl & ALT_FORM)) prefix+=(t>>4), pl=2;
			goto ifmt_tail;
		case 'o':
			a = fmt_o(arg.i, z);
			if ((fl&ALT_FORM) && p<z-a+1) p=z-a+1;
			goto ifmt_tail;
		case 'd': case 'i':
			pl=1;
			if (arg.i>INTMAX_MAX) {
				arg.i=-arg.i;
			} else if (fl & MARK_POS) {
				prefix++;
			} else if (fl & PAD_POS) {
				prefix+=2;
			} else pl=0;
		case 'u':
			a = fmt_u(arg.i, z);
		ifmt_tail:
			if (xp && p<0) goto overflow;
			if (xp) fl &= ~ZERO_PAD;
			if (!arg.i && !p) {
				a=z;
				break;
			}
			p = MAX(p, z-a + !arg.i);
			break;
		narrow_c:
		case 'c':
			*(a=z-(p=1))=arg.i;
			fl &= ~ZERO_PAD;
			break;
		case 'm':
			/* musl vfprintf.c: %m formats strerror(errno); it takes no
			 * argument, so nothing to pop (#657 — was a Phase-1 placeholder). */
			if (1) a = strerror(errno); else
		case 's':
			a = arg.p ? arg.p : (char *)"(null)";
			z = a + strnlen(a, p<0 ? INT_MAX : p);
			if (p<0 && *z) goto overflow;
			p = z-a;
			fl &= ~ZERO_PAD;
			break;
		case 'C':
			/* %lc: a null wide char degrades to a single NUL byte (narrow %c). A
			 * supplementary wint_t (wint_t is 32-bit on every target) becomes a
			 * UTF-16 surrogate PAIR on Windows so the %S loop encodes it as its
			 * 4-byte UTF-8 form; one unit on the UTF-32 targets. wc[3] holds the
			 * pair + NUL. */
			if (!arg.i) goto narrow_c;
			if (WCHAR_UTF16 && arg.i >= 0x10000u) {
				wc[0] = SURR_HIGH(arg.i);
				wc[1] = SURR_LOW(arg.i);
				wc[2] = 0;
			} else {
				wc[0] = arg.i;
				wc[1] = 0;
			}
			arg.p = wc;
			p = -1;
		case 'S':
			/* %ls / %lc: encode each wide char to UTF-8; two passes (measure the
			 * byte length under the precision cap, then pad + emit incrementally),
			 * from musl vfprintf.c. wctomb_pair() combines a UTF-16 surrogate pair
			 * on Windows and folds to plain wctomb on the UTF-32 targets. */
			ws = arg.p;
			for (i=l=0; i<p && *ws && (l=wctomb_pair(mb, &ws))>=0 && l<=p-i; i+=l);
			if (l<0) return -1;
			if (i > INT_MAX) goto overflow;
			p = i;
			pad(f, ' ', w, p, fl);
			ws = arg.p;
			for (i=0; i<0U+p && *ws && i+(l=wctomb_pair(mb, &ws))<=p; i+=l)
				out(f, mb, l);
			pad(f, ' ', w, p, fl^LEFT_ADJ);
			l = w>p ? w : p;
			continue;
		case 'e': case 'f': case 'g': case 'a':
		case 'E': case 'F': case 'G': case 'A':
			if (xp && p<0) goto overflow;
			l = fmt_fp(f, (double)arg.f, w, p, fl, t);
			if (l<0) goto overflow;
			continue;
		}

		if (p < z-a) p = z-a;
		if (p > INT_MAX-pl) goto overflow;
		if (w < pl+p) w = pl+p;
		if (w > INT_MAX-cnt) goto overflow;

		pad(f, ' ', w, pl+p, fl);
		out(f, prefix, pl);
		pad(f, '0', w, pl+p, fl^ZERO_PAD);
		pad(f, '0', p, z-a, 0);
		out(f, a, z-a);
		pad(f, ' ', w, pl+p, fl^LEFT_ADJ);

		l = w;
	}

	if (f) return cnt;
	if (!l10n) return 0;

	for (i=1; i<=NL_ARGMAX && nl_type[i]; i++)
		pop_arg(nl_arg+i, nl_type[i], ap);
	for (; i<=NL_ARGMAX && !nl_type[i]; i++);
	if (i<=NL_ARGMAX) goto inval;
	return 1;

inval:
	errno = EINVAL;
	return -1;
overflow:
	errno = EOVERFLOW;
	return -1;
}

/* ── vfprintf ───────────────────────────────────────────────────────────── */

c2go_extern
int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	va_list ap2;
	int nl_type[NL_ARGMAX+1] = {0};
	union arg nl_arg[NL_ARGMAX+1];
	unsigned char internal_buf[80], *saved_buf = 0;
	int olderr;
	int ret;

	/* musl's va_copy, verbatim. The copy lets us pass a va_list* into
	 * printf_core even where va_list is an array type (x86-64 SysV): ap2 is a
	 * local, so &ap2 is a genuine va_list*, whereas &ap would decay to va_list**
	 * (the `ap` parameter is an array that decays to a pointer). c2go's
	 * __builtin_va_copy is sound on both arches since the #584 fix — AArch64
	 * LowerVACOPY keys the va_list size off the GoABI0 calling convention
	 * (8-byte pointer va_list) instead of the neutral-triple AAPCS 32 bytes. */
	va_copy(ap2, ap);
	if (printf_core(0, fmt, &ap2, nl_arg, nl_type) < 0) {
		va_end(ap2);
		return -1;
	}

	FLOCK(f);
	olderr = f->flags & F_ERR;
	f->flags &= ~F_ERR;
	if (!f->buf_size) {
		/* musl's transient stack buffer, verbatim. Safe under copystack since
		 * the #585 root fix (C2GoFrameAddrRemat): the FILE's pointer fields and
		 * every frame slot reaching them are relocated when the stack moves. */
		saved_buf = f->buf;
		f->buf = internal_buf;
		f->buf_size = sizeof internal_buf;
		f->wpos = f->wbase = f->wend = 0;
	}
	if (!f->wend && __towrite(f)) ret = -1;
	else ret = printf_core(f, fmt, &ap2, nl_arg, nl_type);
	if (saved_buf) {
		f->write(f, 0, 0);
		if (!f->wpos) ret = -1;
		f->buf = saved_buf;
		f->buf_size = 0;
		f->wpos = f->wbase = f->wend = 0;
	}
	if (ferror(f)) ret = -1;
	f->flags |= olderr;
	FUNLOCK(f);
	va_end(ap2);
	return ret;
}

/* ── snprintf / vsnprintf / sprintf / vsprintf (string sinks) ───────────── */

struct cookie {
	char *s;
	size_t n;
};

static size_t sn_write(FILE *f, const unsigned char *s, size_t l)
{
	struct cookie *c = f->cookie;
	size_t k = MIN(c->n, (size_t)(f->wpos - f->wbase));
	if (k) {
		memcpy(c->s, f->wbase, k);
		c->s += k;
		c->n -= k;
	}
	k = MIN(c->n, l);
	if (k) {
		memcpy(c->s, s, k);
		c->s += k;
		c->n -= k;
	}
	*c->s = 0;
	f->wpos = f->wbase = f->buf;
	/* pretend to succeed, even if we discarded extra data */
	return l;
}

c2go_extern
int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
	/* musl vsnprintf.c guard (#657): a size that cannot be represented in the
	 * int return is EOVERFLOW up front. */
	if (n > INT_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	unsigned char buf[1];
	char dummy[1];
	struct cookie c;
	FILE f;
	int r;
	memset(&f, 0, sizeof f);
	c.s = n ? s : dummy;
	c.n = n ? n-1 : 0;
	f.lbf = EOF;
	f.write = sn_write;
	f.lock = -1;
	f.buf = buf;
	f.cookie = &c;

	*c.s = 0;
	r = vfprintf(&f, fmt, ap);
	return r;
}

/* #585 diagnostics getters + deep-stack probe (temporary). */

/* #585 regression probe: drive snprintf's stack sink FILE from many C-recursion
 * depths so morestack/copystack fires at varied points inside the printf chain.
 * Exercised by TestSnprintfDepthSweep (stdio_probe_test.go). */
static int snprobe_rec(char *buf, int n, int depth)
{
	volatile long pad[24];
	for (int i = 0; i < 24; i++) pad[i] = (long)(i + depth);
	if (depth > 0) {
		int r = snprobe_rec(buf, n, depth - 1);
		long s = 0;
		for (int i = 0; i < 24; i++) s += pad[i];
		return r + (int)(s - s);
	}
	return snprintf(buf, n, "%d", 42);
}
c2go_extern int SnprintfProbe(char *buf, int n, int depth) { return snprobe_rec(buf, n, depth); }

c2go_extern int vsprintf(char *restrict s, const char *restrict fmt, va_list ap)
{
	return vsnprintf(s, INT_MAX, fmt, ap);
}

c2go_extern
int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int sprintf(char *restrict s, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsprintf(s, fmt, ap);
	va_end(ap);
	return ret;
}

/* ── dprintf / vdprintf (fd sinks) ──────────────────────────────────────────
 * musl vdprintf.c: an UNBUFFERED (buf_size 0) transient stack FILE over
 * __stdio_write, so each formatted chunk goes straight to write(fd). Same
 * stack-FILE shape as vsnprintf — proven copystack-safe by SnprintfProbe (#585).
 * buf is a non-null placeholder (fmt) that is never dereferenced while buf_size
 * is 0 (__towrite leaves wpos==wbase==wend==buf, so __fwritex flushes directly
 * through f->write). lock=-1: a private FILE, never shared across goroutines. */
c2go_extern
int vdprintf(int fd, const char *restrict fmt, va_list ap)
{
	FILE f;
	memset(&f, 0, sizeof f);
	f.fd = fd;
	f.lbf = EOF;
	f.write = __stdio_write;
	f.buf = (void *)fmt;
	f.buf_size = 0;
	f.lock = -1;
	return vfprintf(&f, fmt, ap);
}

c2go_extern
int dprintf(int fd, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vdprintf(fd, fmt, ap);
	va_end(ap);
	return ret;
}

/* ── asprintf / vasprintf (malloc'd string sink) ────────────────────────────
 * musl vasprintf.c: measure with vsnprintf(0,0), malloc the exact size, format.
 * On measure failure or OOM return -1 (and leave *s unset, per musl). */
c2go_extern
int vasprintf(char **restrict s, const char *restrict fmt, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);
	int l = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (l < 0 || !(*s = malloc(l + 1U))) return -1;
	return vsnprintf(*s, l + 1U, fmt, ap);
}

c2go_extern
int asprintf(char **restrict s, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vasprintf(s, fmt, ap);
	va_end(ap);
	return ret;
}

/* ── printf / fprintf / vprintf ─────────────────────────────────────────── */

c2go_extern
int printf(const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int fprintf(FILE *restrict f, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfprintf(f, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern int vprintf(const char *restrict fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
}

/* ── fputs / puts / fputc / putc / putchar / fflush ─────────────────────── */

c2go_extern
int fputs(const char *restrict s, FILE *restrict f)
{
	size_t l = strlen(s);
	return (fwrite(s, 1, l, f)==l) - 1;
}

/* Drive both writes through unlocked primitives under one FLOCK. The current
 * lock is recursive, but a single scope avoids redundant bridge calls and
 * keeps the two output segments atomic with respect to other writers. */
c2go_extern
int puts(const char *s)
{
	size_t l = strlen(s);
	FLOCK(stdout);
	int r = (__fwritex((const unsigned char *)s, l, stdout)==l &&
	         putc_unlocked('\n', stdout)>=0) ? 0 : -1;
	FUNLOCK(stdout);
	return r;
}

c2go_extern
int fputc(int c, FILE *f)
{
	FLOCK(f);
	c = putc_unlocked(c, f);
	FUNLOCK(f);
	return c;
}

c2go_extern int putc(int c, FILE *f)
{
	FLOCK(f);
	c = putc_unlocked(c, f);
	FUNLOCK(f);
	return c;
}

/* ── perror (musl stdio) ─────────────────────────────────────────────────────
 * Snapshot strerror(errno) BEFORE touching the stream: locking/writing may clobber
 * errno, and perror must report the errno at entry. Then write "msg: errstr\n"
 * (or "errstr\n" when msg is null/empty) to stderr. Like puts, drive the buffer
 * with __fwritex / putc_unlocked directly under one FLOCK. stderr
 * is unbuffered, so every byte reaches fd 2 immediately (no fflush needed). */
c2go_extern void perror(const char *msg)
{
	char *errstr = strerror(errno);
	FILE *f = stderr;

	FLOCK(f);
	if (msg && *msg) {
		__fwritex((const unsigned char *)msg, strlen(msg), f);
		putc_unlocked(':', f);
		putc_unlocked(' ', f);
	}
	__fwritex((const unsigned char *)errstr, strlen(errstr), f);
	putc_unlocked('\n', f);
	FUNLOCK(f);
}

c2go_extern int putchar(int c)
{
	return putc(c, stdout);
}

/* The fflush body without FLOCK/FUNLOCK, so fflush(NULL) can invoke it on each
 * open-file-list entry while it already holds that file's stdio lock. */
static int __fflush_unlocked(FILE *f)
{
	/* If writing, flush output */
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return EOF;
	}

	/* If reading, sync position, per POSIX */
	if (f->rpos != f->rend) f->seek(f, f->rpos-f->rend, SEEK_CUR);

	/* Clear read and write modes */
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;

	return 0;
}

c2go_extern int fflush(FILE *f)
{
	if (!f) {
		int r = 0;
		r |= fflush(stdout);
		r |= fflush(stderr);

		/* Flush every writing stream on the open-file list (fopen'd files).
		 * Call the unlocked body inside the existing FLOCK to avoid a redundant
		 * recursive bridge. The ofl lock is distinct, so holding it while
		 * taking each file's stdio lock cannot self-deadlock. */
		for (f = *__ofl_lock(); f; f = f->next) {
			FLOCK(f);
			if (f->wpos != f->wbase) r |= __fflush_unlocked(f);
			FUNLOCK(f);
		}
		__ofl_unlock();

		return r;
	}

	FLOCK(f);
	int r = __fflush_unlocked(f);
	FUNLOCK(f);
	return r;
}

/* ── fopen family (#594) ─────────────────────────────────────────────────────
 * Ported from musl's fopen/__fdopen/fclose/__fmodeflags + ofl (the open-file
 * list). FILE + its buffer are one plain malloc block (unmanaged memory; the
 * FILE holds no GC pointers). Deviations from musl, all justified:
 *   - Terminal line-buffering: stdout auto-detects a tty on its first write
 *     (__stdout_write) and __fdopen probes with isatty (#661) — both stand in
 *     for musl's ioctl TIOCGWINSZ. On Windows stdout stays fully buffered.
 *   - No fcntl: open() already applies O_CLOEXEC / O_APPEND via __fmodeflags,
 *     so musl's redundant post-open fcntl(F_SETFD)/fcntl(F_SETFL) are dropped.
 *     F_APP is set from the mode for parity but is inert (no write path reads
 *     it; the kernel's O_APPEND does the work).
 *   - The open-file list uses a distinct global lock (_c2go_ofl_lock), NOT the
 *     stdio lock, so fflush(NULL) can walk it while re-locking each file.
 *   - freopen is implemented below, calling dup3 (io.go's fd layer) exactly as
 *     musl's freopen does; on macOS (no dup3 syscall) Dup3 falls back to dup2 +
 *     fcntl(FD_CLOEXEC), on Linux it is the native atomic dup3.
 * Program exit flushes buffered output: the c2go entry wrapper terminates via C
 * exit(), which runs __c2go_finalize (atexit handlers, then fflush(NULL)). A
 * hand-written Go host that bypasses that wrapper (its own func main / os.Exit)
 * should call C2goFinalize — or fflush — before exiting, or buffered stdout is
 * lost. Note two userspace buffers share each std fd: C's buffered stdout is NOT
 * ordered with Go's os.Stdout or a system() child writing fd 1 (same hazard as
 * mixing printf and write(1) in C — fflush(stdout) before handing the fd off),
 * and C stdin's read-ahead can hold bytes an os.Stdin reader then misses. */

/* musl __fmodeflags.c, verbatim. */
static int __fmodeflags(const char *mode)
{
	int flags;
	if (strchr(mode, '+')) flags = O_RDWR;
	else if (*mode == 'r') flags = O_RDONLY;
	else flags = O_WRONLY;
	if (strchr(mode, 'x')) flags |= O_EXCL;
	if (strchr(mode, 'e')) flags |= O_CLOEXEC;
	if (*mode != 'r') flags |= O_CREAT;
	if (*mode == 'w') flags |= O_TRUNC;
	if (*mode == 'a') flags |= O_APPEND;
	return flags;
}

/* Open-file list (musl ofl.c / ofl_add.c). Guarded by the dedicated ofl lock. */
static FILE *ofl_head;

static FILE **__ofl_lock(void)
{
	_c2go_ofl_lock();
	return &ofl_head;
}

static void __ofl_unlock(void)
{
	_c2go_ofl_unlock();
}

static FILE *__ofl_add(FILE *f)
{
	FILE **head = __ofl_lock();
	f->next = *head;
	if (*head) (*head)->prev = f;
	*head = f;
	__ofl_unlock();
	return f;
}

/* Initialize a caller-owned raw FILE engine. The storage and its buffer may
 * come from either root libc's unmanaged allocator or mlib's Go heap; this
 * helper only installs musl's descriptor operations and scalar state. `lock`
 * is 0 for the root carrier and -1 when an outer managed wrapper owns locking.
 */
static int __file_init_fd(FILE *f, int fd, const char *mode,
	unsigned char *storage, size_t storage_size, int lock)
{
	if (!strchr("rwa", *mode) || storage_size < UNGET) {
		errno = EINVAL;
		return -1;
	}

	memset(f, 0, sizeof *f);
	if (!strchr(mode, '+')) f->flags = (*mode == 'r') ? F_NOWR : F_NORD;
	if (*mode == 'a') f->flags |= F_APP;

	f->fd = fd;
	f->buf = storage + UNGET;
	f->buf_size = storage_size - UNGET;
	f->lbf = EOF;
#if !defined(_WIN32)
	if (!(f->flags & F_NOWR)) {
		int e_ = errno;
		if (isatty(fd)) f->lbf = '\n';
		errno = e_;
	}
#endif
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;
	f->lock = lock;
	return 0;
}

/* musl __fdopen.c, minus the ioctl/fcntl bits (see the section note). */
static FILE *__fdopen(int fd, const char *mode)
{
	FILE *f;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+buffer or fail */
	if (!(f = malloc(sizeof *f + UNGET + BUFSIZ))) return 0;

	/* lock=0 engages root libc's handle-backed per-FILE lock. */
	(void)__file_init_fd(f, fd, mode, (unsigned char *)f + sizeof *f,
		UNGET + BUFSIZ, 0);

	/* Add new FILE to open file list */
	return __ofl_add(f);
}

/* Internal cross-package entry points for mlib's managed FILE carrier. They
 * deliberately do not allocate, join root libc's open-file list, or touch the
 * root fileLockTab. The caller supplies a separately rooted buffer and owns
 * synchronization around every operation on the raw engine. */
c2go_extern int __c2go_file_raw_fdopen(FILE *f, int fd, const char *mode,
	unsigned char *storage, size_t storage_size)
{
	return __file_init_fd(f, fd, mode, storage, storage_size, -1);
}

c2go_extern int __c2go_file_raw_open(FILE *f, const char *filename,
	const char *mode, unsigned char *storage, size_t storage_size)
{
	int fd;
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return -1;
	}
	fd = open(filename, __fmodeflags(mode), 0666);
	if (fd < 0) return -1;
	if (__file_init_fd(f, fd, mode, storage, storage_size, -1) == 0)
		return 0;
	close(fd);
	return -1;
}

/* Initialize a caller-owned standard-stream engine. It has the same live
 * os.Stdin/os.Stdout/os.Stderr routing and buffering policy as root libc's
 * static FILE objects, but no root FILE lock or open-list membership. */
c2go_extern int __c2go_file_raw_stdinit(FILE *f, int which,
	unsigned char *storage, size_t storage_size)
{
	const char *mode;
	if ((unsigned)which > 2) {
		errno = EINVAL;
		return -1;
	}
	mode = which == 0 ? "r" : "w";
	if (__file_init_fd(f, which, mode, storage, storage_size, -1) != 0)
		return -1;

	f->flags = F_PERM | (which == 0 ? F_NOWR : F_NORD);
	f->close = __std_go_close;
	if (which == 0) {
		f->write = 0;
		f->lbf = EOF;
	} else {
		f->read = 0;
		if (which == 1) {
			f->write = __stdout_write;
			f->lbf = '\n';
		} else {
			f->write = __std_go_write;
			f->buf_size = 0;
			f->lbf = EOF;
		}
	}
	return 0;
}

/* musl fopen.c, minus the redundant post-open CLOEXEC fcntl. (fopen is a clang
 * LibFunc, so clang emits a benign "goabi0 CC not supported on builtin function"
 * warning here — the emitted symbol is still a correct goabi0cc ·fopen. No
 * no_builtin is needed: the stdio.h prototype (goabi0) is always in scope, so
 * clang never synthesises a clashing implicit non-CC declaration — verified.) */
c2go_extern
FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	flags = __fmodeflags(mode);

	fd = open(filename, flags, 0666);
	if (fd < 0) return 0;

	f = __fdopen(fd, mode);
	if (f) return f;

	close(fd);
	return 0;
}

c2go_extern
FILE *fdopen(int fd, const char *mode)
{
	return __fdopen(fd, mode);
}

/* musl fclose.c. The per-thread locked-file list (__unlist_locked_file) has no
 * analogue in the Go-backed lock, so it is omitted. fflush is inlined as
 * __fflush_unlocked inside the stream's existing lock scope. */
c2go_extern
int fclose(FILE *f)
{
	int r;

	FLOCK(f);
	r = __fflush_unlocked(f);
	r |= f->close(f);
	FUNLOCK(f);

	if (f->flags & F_PERM) return r;

	/* Remove from the open-file list. */
	FILE **head = __ofl_lock();
	if (f->prev) f->prev->next = f->next;
	if (f->next) f->next->prev = f->prev;
	if (*head == f) *head = f->next;
	__ofl_unlock();

	_c2go_file_lock_drop(&f->lockid); /* #659: release the per-FILE lock root */
	free(f->getln_buf);
	free(f);

	return r;
}

/* mlib counterpart of fclose's engine portion. The managed wrapper has
 * already locked and retired its public carrier; storage reclamation belongs
 * to Go's GC, so this helper only flushes and closes the descriptor. */
c2go_extern int __c2go_file_raw_close(FILE *f)
{
	int r = __fflush_unlocked(f);
	r |= f->close(f);
	return r;
}

#if defined(_WIN32)
/* freopen(std)'s Windows rebind (#644): _get_osfhandle recovers the WIN32
 * HANDLE behind the temporary stream's CRT fd (msvcrt import, LLP64
 * intptr_t = long long); the Go shim duplicates it and assigns a fresh
 * os.File to the os.Std* variable (../stdio_std_windows.go). */
extern long long _get_osfhandle(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_rebind", C2GO_GOABI0)
int __c2go_std_rebind(int which, long long h);
#endif

/* ── popen / pclose (#662, cross-platform since #678) ────────────────────────
 * musl popen.c is fork+dup2 then fdopen; the spawn here rides the Go bridge
 * (os/exec, like system()) which hands back a C-EXCLUSIVE pipe fd — on
 * windows the bridge converts the HANDLE via _open_osfhandle so this fd IS a
 * CRT fd (popen_windows.go). fdopen wraps it exactly as musl does. The FILE
 * carries the Go handle in pipe_id; pclose fclose()s the stream (child sees
 * EOF/SIGPIPE) then Wait()s: raw wait status on unix (sys/wait.h macros),
 * exit code on windows (the _pclose shape). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_popen", C2GO_GOABI0)
long long __c2go_popen(const char *command, int write, int *fdout);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pclose", C2GO_GOABI0)
long long __c2go_pclose(long long id);

c2go_extern FILE *popen(const char *command, const char *mode)
{
	if (!mode || (mode[0] != 'r' && mode[0] != 'w')) {
		errno = EINVAL;
		return 0;
	}
	int fd = -1;
	long long id = __c2go_popen(command, mode[0] == 'w', &fd);
	if (id < 0) {
		errno = (int)-id;
		return 0;
	}
	FILE *f = fdopen(fd, mode);
	if (!f) {
		close(fd);
		__c2go_pclose(id); /* reap the child; its pipe end EOFs on our close */
		return 0;
	}
	f->pipe_id = (unsigned long long)id;
	return f;
}

c2go_extern int pclose(FILE *f)
{
	long long id = (long long)f->pipe_id;
	if (!id) {
		errno = EINVAL; /* not a popen stream */
		return -1;
	}
	fclose(f); /* flush + close the pipe end: the child sees EOF / SIGPIPE */
	long long st = __c2go_pclose(id);
	if (st < 0) {
		errno = (int)-st;
		return -1;
	}
	return (int)st; /* raw wait status, musl/waitpid encoding */
}

/* musl freopen.c. The stream object f is reused: its fd is repointed at the
 * newly opened file (kept at the same descriptor number via dup3) and its I/O
 * ops are copied from the temporary stream. Lock-order adaptations:
 *   - fflush(f) is the unlocked __fflush_unlocked (fclose does the same).
 *   - fopen(filename) runs BEFORE FLOCK(f): fopen takes the ofl lock, and
 *     fflush(NULL) takes ofl-then-stdio, so opening under FLOCK(f)
 *     (stdio-then-ofl) would invert that lock order (ABBA).
 *   - fclose(f2) runs AFTER FUNLOCK(f), avoiding an unnecessary nested lock.
 * (freopen is a clang LibFunc like fopen — same benign builtin-CC warning, no
 * no_builtin needed; see fopen.) */
c2go_extern
FILE *freopen(const char *restrict filename, const char *restrict mode,
              FILE *restrict f)
{
#if !defined(_WIN32)
	int fl = __fmodeflags(mode);   /* O_* flags freopen adjusts — unix fcntl/dup3 only */
#endif
	FILE *f2;

	/* filename == NULL: change the flags of f's existing fd in place (a POSIX-2008
	 * extension). Unsupported on Windows — there is no fcntl (MinGW-w64 provides
	 * none) and a CRT fd's flags cannot change after open (MSVCRT freopen likewise
	 * requires a real filename). Reject, closing f per freopen's failure contract
	 * (std streams excepted — see the std gate below). */
	if (!filename) {
#if defined(_WIN32)
		errno = EINVAL;
		if (!(f == stdin || f == stdout || f == stderr))
			fclose(f);
		return 0;
#else
		FLOCK(f);
		__fflush_unlocked(f);
		/* Std streams: only while un-swapped (identity gate, same policy as the
		 * filename path below); the fd-layer fcntl routes the virtualized 0/1/2.
		 * Refuse WITHOUT killing the std stream. */
		if ((f == stdin || f == stdout || f == stderr) &&
		    !__c2go_std_isdefault(f->fd)) {
			FUNLOCK(f);
			errno = EINVAL;
			return 0;
		}
		if (fl & O_CLOEXEC)
			fcntl(f->fd, F_SETFD, FD_CLOEXEC);
		fl &= ~(O_CREAT | O_EXCL | O_CLOEXEC);
		if (fcntl(f->fd, F_SETFL, fl) < 0) {
			FUNLOCK(f);
			fclose(f);
			return 0;
		}
		FUNLOCK(f);
		return f;
#endif
	}

	f2 = fopen(filename, mode);
	if (!f2) {
		fclose(f);
		return 0;
	}

	FLOCK(f);
	__fflush_unlocked(f);

	/* Std streams (virtualized 0/1/2): a kernel-level redirect is coherent only
	 * while the stream is still the STARTUP os.Std* object — then its kernel fd
	 * IS the literal 0/1/2, the plain dup2/dup3 below does the POSIX-faithful
	 * repoint, and os.Std* (wrapping that fd) follows, as do runtime panic
	 * writes and exec'd children. After a Go-side swap, repointing the fd under
	 * the host-owned *os.File would desync its runtime state — refuse, and
	 * DEVIATE from freopen's failure contract by NOT closing the std stream (an
	 * unchecked freopen(...,stdout) must not kill C stdout; it keeps routing to
	 * the live os.Std*). On Windows the redirect is a REBIND instead (#644):
	 * os.Std* is HANDLE-backed (no CRT dup2 can move it), so f2's underlying
	 * HANDLE is duplicated into a fresh os.File assigned to the os.Std*
	 * VARIABLE — the design's sanctioned Go-side redirect, which mutates
	 * nothing under a host object and thus needs no identity gate. */
	int is_std = (f == stdin || f == stdout || f == stderr);
#if defined(_WIN32)
	if (is_std) {
		long long h = _get_osfhandle(f2->fd);
		if (h == -1 || __c2go_std_rebind(f->fd, h) < 0) {
			FUNLOCK(f);
			fclose(f2);
			errno = EINVAL;
			return 0;    /* std stream survives the failed redirect */
		}
		/* The Go side duplicated the handle: fclose(f2) below releases the CRT
		 * fd + original handle normally; os.Std* owns the duplicate. */
	} else if (f2->fd == f->fd) {
		f2->fd = -1; /* fopen reused f's number: don't let fclose(f2) close it */
	/* Windows / MinGW-w64 has no dup3 (a Linux syscall) — use dup2. The only
	 * thing dup3 adds, an atomic close-on-exec on the copy, is moot here (this
	 * runtime has no fork/exec) and unexpressible on a CRT fd anyway. */
	} else if (dup2(f2->fd, f->fd) < 0) {
#else
	if (is_std && !__c2go_std_isdefault(f->fd)) {
		FUNLOCK(f);
		fclose(f2);
		errno = EINVAL;
		return 0;
	}
	if (f2->fd == f->fd) {
		f2->fd = -1; /* fopen reused f's number: don't let fclose(f2) close it */
	} else if (dup3(f2->fd, f->fd, fl & O_CLOEXEC) < 0) {
#endif
		FUNLOCK(f);
		fclose(f2);
		fclose(f);
		return 0;
	}

	f->flags = (f->flags & F_PERM) | f2->flags;
	/* Std streams keep their live-os.Std* ops: the kernel fd behind os.Std*
	 * was just repointed, so the routed stream follows the redirect; copying
	 * f2's raw-fd ops would freeze it onto today's fd number. */
	if (!is_std) {
		f->read = f2->read;
		f->write = f2->write;
		f->seek = f2->seek;
		f->close = f2->close;
	}

	FUNLOCK(f);
	fclose(f2);
	return f;
}

/* ── scanf family (#583) ─────────────────────────────────────────────────────
 * Ported from musl's vfscanf.c + internal/intscan.c + internal/shgetc.c + the
 * FILE read helpers (__toread/__uflow). The scanf CORE: integer conversions
 * (%d/%i/%o/%u/%x/%p), string/char/scanset (%s/%c/%[), and %n. Adaptations from
 * musl (faithful otherwise):
 *   - Wide-char conversions (%ls/%lc/%l[, %C/%S) decode input bytes to wchar_t
 *     via __mbrtoc32; on Windows a supplementary scalar is stored as a UTF-16
 *     surrogate PAIR (a per-unit mbrtowc alone would EILSEQ it).
 *   - The %m (POSIX malloc-the-result) extension and positional %n$ arguments
 *     are ported from musl (#660; arg_n sits just above vfscanf, shared with
 *     the wide vfwscanf below). The only %m deviation: on the UTF-16 wchar_t
 *     target a supplementary scalar appends a surrogate PAIR, so the
 *     alloc-grow check runs BEFORE the append (musl grows after appending a
 *     single unit, which a two-unit append could overshoot).
 *   - Float conversions (%a/%A/%e/%E/%f/%F/%g/%G) go through __floatscan (#589),
 *     with long double taken as double (arm64 repr; x86 80-bit unsupported,
 *     same as fmt_fp). %f stores a float, %lf a double, %Lf a double.
 */

/* musl __toread.c (dropped the __stdio_exit hook — no atexit flushing here). */
static int __toread(FILE *f)
{
	f->mode |= f->mode-1;
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	f->wpos = f->wbase = f->wend = 0;
	if (f->flags & F_NORD) {
		f->flags |= F_ERR;
		return EOF;
	}
	f->rpos = f->rend = f->buf + f->buf_size;
	return (f->flags & F_EOF) ? EOF : 0;
}

/* musl __uflow.c: pull one byte when the read buffer is empty. */
static int __uflow(FILE *f)
{
	unsigned char c;
	if (!__toread(f) && f->read(f, &c, 1)==1) return c;
	return EOF;
}

/* musl internal/shgetc.h shcnt macro (defined before __shgetc, which uses it). */
#define shcnt(f) ((f)->shcnt + ((f)->rpos - (f)->buf))

/* musl internal/shgetc.c — the scan-helper stream limiter (shlim) and the
 * buffer-refill slow path (shgetc). */
static void __shlim(FILE *f, off_t lim)
{
	f->shlim = lim;
	f->shcnt = f->buf - f->rpos;
	/* If lim is nonzero, rend must be a valid pointer. */
	if (lim && f->rend - f->rpos > lim)
		f->shend = f->rpos + lim;
	else
		f->shend = f->rend;
}

static int __shgetc(FILE *f)
{
	int c;
	off_t cnt = shcnt(f);
	if (f->shlim && cnt >= f->shlim || (c=__uflow(f)) < 0) {
		f->shcnt = f->buf - f->rpos + cnt;
		f->shend = f->rpos;
		f->shlim = -1;
		return EOF;
	}
	cnt++;
	if (f->shlim && f->rend - f->rpos > f->shlim - cnt)
		f->shend = f->rpos + (f->shlim - cnt);
	else
		f->shend = f->rend;
	f->shcnt = f->buf - f->rpos + cnt;
	if (f->rpos <= f->buf) f->rpos[-1] = c;
	return c;
}

#define shlim(f, lim) __shlim((f), (lim))
#define shgetc(f) (((f)->rpos != (f)->shend) ? *(f)->rpos++ : __shgetc(f))
#define shunget(f) ((f)->shlim>=0 ? (void)(f)->rpos-- : (void)0)

/* musl internal/intscan.c — digit lookup, offset by 1 so val[-1] (EOF, c==-1)
 * maps to table[0] == 255 (>= any base → stops the scan). */
static const unsigned char intscan_table[] = { (unsigned char)-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static unsigned long long __intscan(FILE *f, unsigned base, int pok, unsigned long long lim)
{
	const unsigned char *val = intscan_table+1;
	int c, neg=0;
	unsigned x;
	unsigned long long y;
	if (base > 36 || base == 1) {
		errno = EINVAL;
		return 0;
	}
	while (isspace((c=shgetc(f))));
	if (c=='+' || c=='-') {
		neg = -(c=='-');
		c = shgetc(f);
	}
	if ((base == 0 || base == 16) && c=='0') {
		c = shgetc(f);
		if ((c|32)=='x') {
			c = shgetc(f);
			if (val[c]>=16) {
				shunget(f);
				if (pok) shunget(f);
				else shlim(f, 0);
				return 0;
			}
			base = 16;
		} else if (base == 0) {
			base = 8;
		}
	} else {
		if (base == 0) base = 10;
		if (val[c] >= base) {
			shunget(f);
			shlim(f, 0);
			errno = EINVAL;
			return 0;
		}
	}
	if (base == 10) {
		for (x=0; c-'0'<10U && x<=UINT_MAX/10-1; c=shgetc(f))
			x = x*10 + (c-'0');
		for (y=x; c-'0'<10U && y<=ULLONG_MAX/10 && 10*y<=ULLONG_MAX-(c-'0'); c=shgetc(f))
			y = y*10 + (c-'0');
		if (c-'0'>=10U) goto done;
	} else if (!(base & base-1)) {
		int bs = "\0\1\2\4\7\3\6\5"[(0x17*base)>>5&7];
		for (x=0; val[c]<base && x<=UINT_MAX/32; c=shgetc(f))
			x = x<<bs | val[c];
		for (y=x; val[c]<base && y<=ULLONG_MAX>>bs; c=shgetc(f))
			y = y<<bs | val[c];
	} else {
		for (x=0; val[c]<base && x<=UINT_MAX/36-1; c=shgetc(f))
			x = x*base + val[c];
		for (y=x; val[c]<base && y<=ULLONG_MAX/base && base*y<=ULLONG_MAX-val[c]; c=shgetc(f))
			y = y*base + val[c];
	}
	if (val[c]<base) {
		for (; val[c]<base; c=shgetc(f));
		errno = ERANGE;
		y = lim;
		if (lim&1) neg = 0;
	}
done:
	shunget(f);
	if (y>=lim) {
		if (!(lim&1) && !neg) {
			errno = ERANGE;
			return lim-1;
		} else if (y>lim) {
			errno = ERANGE;
			return lim;
		}
	}
	return (y^neg)-neg;
}

/* musl vfscanf.c store_int (float/wide stores dropped — see the header note). */
#define SIZE_hh -2
#define SIZE_h  -1
#define SIZE_def 0
#define SIZE_l   1
#define SIZE_L   2
#define SIZE_ll  3

static void store_int(void *dest, int size, unsigned long long i)
{
	if (!dest) return;
	switch (size) {
	case SIZE_hh:
		*(char *)dest = i;
		break;
	case SIZE_h:
		*(short *)dest = i;
		break;
	case SIZE_def:
		*(int *)dest = i;
		break;
	case SIZE_l:
		*(long *)dest = i;
		break;
	case SIZE_ll:
		*(long long *)dest = i;
		break;
	}
}

/* ── __floatscan (#589) — musl internal/floatscan.c ──────────────────────────
 * long double is taken as double (the arm64 representation; x86 80-bit is
 * unsupported, same as fmt_fp). The FS_* values are the LDBL_MANT_DIG==53
 * branch of musl's float.h switch. Math deps: fp_fmod/fp_scalbn (ported above),
 * __builtin_copysign/fabs/inf/nan (all lower inline / to no external symbol). */
#define FS_MANT_DIG      53                        /* LDBL_MANT_DIG */
#define FS_MIN_EXP       (-1021)                   /* LDBL_MIN_EXP */
#define FS_EPSILON       0x1p-52                   /* LDBL_EPSILON */
#define FS_MAX           0x1.fffffffffffffp+1023   /* LDBL_MAX */
#define FS_MIN           0x1p-1022                 /* LDBL_MIN */
#define FS_B1B_DIG       2                         /* LD_B1B_DIG */
#define FS_KMAX          128
#define FS_MASK          (FS_KMAX-1)
#define FS_FLT_MANT_DIG  24
#define FS_FLT_MIN_EXP   (-125)
#define FS_DBL_MANT_DIG  53
#define FS_DBL_MIN_EXP   (-1021)

static long long scanexp(FILE *f, int pok)
{
	int c;
	int x;
	long long y;
	int neg = 0;

	c = shgetc(f);
	if (c=='+' || c=='-') {
		neg = (c=='-');
		c = shgetc(f);
		if (c-'0'>=10U && pok) shunget(f);
	}
	if (c-'0'>=10U) {
		shunget(f);
		return LLONG_MIN;
	}
	for (x=0; c-'0'<10U && x<INT_MAX/10; c = shgetc(f))
		x = 10*x + c-'0';
	for (y=x; c-'0'<10U && y<LLONG_MAX/100; c = shgetc(f))
		y = 10*y + c-'0';
	for (; c-'0'<10U; c = shgetc(f));
	shunget(f);
	return neg ? -y : y;
}

static double decfloat(FILE *f, int c, int bits, int emin, int sign, int pok)
{
	uint32_t x[FS_KMAX];
	static const uint32_t th[] = { 9007199, 254740991 };  /* LD_B1B_MAX */
	int i, j, k, a, z;
	long long lrp=0, dc=0;
	long long e10=0;
	int lnz = 0;
	int gotdig = 0, gotrad = 0;
	int rp;
	int e2;
	int emax = -emin-bits+3;
	int denormal = 0;
	double y;
	double frac=0;
	double bias=0;
	static const int p10s[] = { 10, 100, 1000, 10000,
		100000, 1000000, 10000000, 100000000 };

	j=0;
	k=0;

	/* Don't let leading zeros consume buffer space */
	for (; c=='0'; c = shgetc(f)) gotdig=1;
	if (c=='.') {
		gotrad = 1;
		for (c = shgetc(f); c=='0'; c = shgetc(f)) gotdig=1, lrp--;
	}

	x[0] = 0;
	for (; c-'0'<10U || c=='.'; c = shgetc(f)) {
		if (c == '.') {
			if (gotrad) break;
			gotrad = 1;
			lrp = dc;
		} else if (k < FS_KMAX-3) {
			dc++;
			if (c!='0') lnz = dc;
			if (j) x[k] = x[k]*10 + c-'0';
			else x[k] = c-'0';
			if (++j==9) {
				k++;
				j=0;
			}
			gotdig=1;
		} else {
			dc++;
			if (c!='0') {
				lnz = (FS_KMAX-4)*9;
				x[FS_KMAX-4] |= 1;
			}
		}
	}
	if (!gotrad) lrp=dc;

	if (gotdig && (c|32)=='e') {
		e10 = scanexp(f, pok);
		if (e10 == LLONG_MIN) {
			if (pok) {
				shunget(f);
			} else {
				shlim(f, 0);
				return 0;
			}
			e10 = 0;
		}
		lrp += e10;
	} else if (c>=0) {
		shunget(f);
	}
	if (!gotdig) {
		errno = EINVAL;
		shlim(f, 0);
		return 0;
	}

	/* Handle zero specially to avoid nasty special cases later */
	if (!x[0]) return sign * 0.0;

	/* Optimize small integers (w/no exponent) and over/under-flow */
	if (lrp==dc && dc<10 && (bits>30 || x[0]>>bits==0))
		return sign * (double)x[0];
	if (lrp > -emin/2) {
		errno = ERANGE;
		return sign * FS_MAX * FS_MAX;
	}
	if (lrp < emin-2*FS_MANT_DIG) {
		errno = ERANGE;
		return sign * FS_MIN * FS_MIN;
	}

	/* Align incomplete final B1B digit */
	if (j) {
		for (; j<9; j++) x[k]*=10;
		k++;
		j=0;
	}

	a = 0;
	z = k;
	e2 = 0;
	rp = lrp;

	/* Optimize small to mid-size integers (even in exp. notation) */
	if (lnz<9 && lnz<=rp && rp < 18) {
		if (rp == 9) return sign * (double)x[0];
		if (rp < 9) return sign * (double)x[0] / p10s[8-rp];
		int bitlim = bits-3*(int)(rp-9);
		if (bitlim>30 || x[0]>>bitlim==0)
			return sign * (double)x[0] * p10s[rp-10];
	}

	/* Drop trailing zeros */
	for (; !x[z-1]; z--);

	/* Align radix point to B1B digit boundary */
	if (rp % 9) {
		int rpm9 = rp>=0 ? rp%9 : rp%9+9;
		int p10 = p10s[8-rpm9];
		uint32_t carry = 0;
		for (k=a; k!=z; k++) {
			uint32_t tmp = x[k] % p10;
			x[k] = x[k]/p10 + carry;
			carry = 1000000000/p10 * tmp;
			if (k==a && !x[k]) {
				a = (a+1 & FS_MASK);
				rp -= 9;
			}
		}
		if (carry) x[z++] = carry;
		rp += 9-rpm9;
	}

	/* Upscale until desired number of bits are left of radix point */
	while (rp < 9*FS_B1B_DIG || (rp == 9*FS_B1B_DIG && x[a]<th[0])) {
		uint32_t carry = 0;
		e2 -= 29;
		for (k=(z-1 & FS_MASK); ; k=(k-1 & FS_MASK)) {
			uint64_t tmp = ((uint64_t)x[k] << 29) + carry;
			if (tmp > 1000000000) {
				carry = tmp / 1000000000;
				x[k] = tmp % 1000000000;
			} else {
				carry = 0;
				x[k] = tmp;
			}
			if (k==(z-1 & FS_MASK) && k!=a && !x[k]) z = k;
			if (k==a) break;
		}
		if (carry) {
			rp += 9;
			a = (a-1 & FS_MASK);
			if (a == z) {
				z = (z-1 & FS_MASK);
				x[z-1 & FS_MASK] |= x[z];
			}
			x[a] = carry;
		}
	}

	/* Downscale until exactly number of bits are left of radix point */
	for (;;) {
		uint32_t carry = 0;
		int sh = 1;
		for (i=0; i<FS_B1B_DIG; i++) {
			k = (a+i & FS_MASK);
			if (k == z || x[k] < th[i]) {
				i=FS_B1B_DIG;
				break;
			}
			if (x[a+i & FS_MASK] > th[i]) break;
		}
		if (i==FS_B1B_DIG && rp==9*FS_B1B_DIG) break;
		/* FIXME: find a way to compute optimal sh */
		if (rp > 9+9*FS_B1B_DIG) sh = 9;
		e2 += sh;
		for (k=a; k!=z; k=(k+1 & FS_MASK)) {
			uint32_t tmp = x[k] & (1<<sh)-1;
			x[k] = (x[k]>>sh) + carry;
			carry = (1000000000>>sh) * tmp;
			if (k==a && !x[k]) {
				a = (a+1 & FS_MASK);
				i--;
				rp -= 9;
			}
		}
		if (carry) {
			if ((z+1 & FS_MASK) != a) {
				x[z] = carry;
				z = (z+1 & FS_MASK);
			} else x[z-1 & FS_MASK] |= 1;
		}
	}

	/* Assemble desired bits into floating point variable */
	for (y=i=0; i<FS_B1B_DIG; i++) {
		if ((a+i & FS_MASK)==z) x[(z=(z+1 & FS_MASK))-1] = 0;
		y = 1000000000.0 * y + x[a+i & FS_MASK];
	}

	y *= sign;

	/* Limit precision for denormal results */
	if (bits > FS_MANT_DIG+e2-emin) {
		bits = FS_MANT_DIG+e2-emin;
		if (bits<0) bits=0;
		denormal = 1;
	}

	/* Calculate bias term to force rounding, move out lower bits */
	if (bits < FS_MANT_DIG) {
		bias = __builtin_copysign(fp_scalbn(1, 2*FS_MANT_DIG-bits-1), y);
		frac = fp_fmod(y, fp_scalbn(1, FS_MANT_DIG-bits));
		y -= frac;
		y += bias;
	}

	/* Process tail of decimal input so it can affect rounding */
	if ((a+i & FS_MASK) != z) {
		uint32_t t = x[a+i & FS_MASK];
		if (t < 500000000 && (t || (a+i+1 & FS_MASK) != z))
			frac += 0.25*sign;
		else if (t > 500000000)
			frac += 0.75*sign;
		else if (t == 500000000) {
			if ((a+i+1 & FS_MASK) == z)
				frac += 0.5*sign;
			else
				frac += 0.75*sign;
		}
		if (FS_MANT_DIG-bits >= 2 && !fp_fmod(frac, 1))
			frac++;
	}

	y += frac;
	y -= bias;

	if ((e2+FS_MANT_DIG & INT_MAX) > emax-5) {
		if (__builtin_fabs(y) >= 2/FS_EPSILON) {
			if (denormal && bits==FS_MANT_DIG+e2-emin)
				denormal = 0;
			y *= 0.5;
			e2++;
		}
		if (e2+FS_MANT_DIG>emax || (denormal && frac))
			errno = ERANGE;
	}

	return fp_scalbn(y, e2);
}

static double hexfloat(FILE *f, int bits, int emin, int sign, int pok)
{
	uint32_t x = 0;
	double y = 0;
	double scale = 1;
	double bias = 0;
	int gottail = 0, gotrad = 0, gotdig = 0;
	long long rp = 0;
	long long dc = 0;
	long long e2 = 0;
	int d;
	int c;

	c = shgetc(f);

	/* Skip leading zeros */
	for (; c=='0'; c = shgetc(f)) gotdig = 1;

	if (c=='.') {
		gotrad = 1;
		c = shgetc(f);
		/* Count zeros after the radix point before significand */
		for (rp=0; c=='0'; c = shgetc(f), rp--) gotdig = 1;
	}

	for (; c-'0'<10U || (c|32)-'a'<6U || c=='.'; c = shgetc(f)) {
		if (c=='.') {
			if (gotrad) break;
			rp = dc;
			gotrad = 1;
		} else {
			gotdig = 1;
			if (c > '9') d = (c|32)+10-'a';
			else d = c-'0';
			if (dc<8) {
				x = x*16 + d;
			} else if (dc < FS_MANT_DIG/4+1) {
				y += d*(scale/=16);
			} else if (d && !gottail) {
				y += 0.5*scale;
				gottail = 1;
			}
			dc++;
		}
	}
	if (!gotdig) {
		shunget(f);
		if (pok) {
			shunget(f);
			if (gotrad) shunget(f);
		} else {
			shlim(f, 0);
		}
		return sign * 0.0;
	}
	if (!gotrad) rp = dc;
	while (dc<8) x *= 16, dc++;
	if ((c|32)=='p') {
		e2 = scanexp(f, pok);
		if (e2 == LLONG_MIN) {
			if (pok) {
				shunget(f);
			} else {
				shlim(f, 0);
				return 0;
			}
			e2 = 0;
		}
	} else {
		shunget(f);
	}
	e2 += 4*rp - 32;

	if (!x) return sign * 0.0;
	if (e2 > -emin) {
		errno = ERANGE;
		return sign * FS_MAX * FS_MAX;
	}
	if (e2 < emin-2*FS_MANT_DIG) {
		errno = ERANGE;
		return sign * FS_MIN * FS_MIN;
	}

	while (x < 0x80000000) {
		if (y>=0.5) {
			x += x + 1;
			y += y - 1;
		} else {
			x += x;
			y += y;
		}
		e2--;
	}

	if (bits > 32+e2-emin) {
		bits = 32+e2-emin;
		if (bits<0) bits=0;
	}

	if (bits < FS_MANT_DIG)
		bias = __builtin_copysign(fp_scalbn(1, 32+FS_MANT_DIG-bits-1), sign);

	if (bits<32 && y && !(x&1)) x++, y=0;

	y = bias + sign*(double)x + sign*y;
	y -= bias;

	if (!y) errno = ERANGE;

	return fp_scalbn(y, e2);
}

static double __floatscan(FILE *f, int prec, int pok)
{
	int sign = 1;
	size_t i;
	int bits;
	int emin;
	int c;

	switch (prec) {
	case 0:
		bits = FS_FLT_MANT_DIG;
		emin = FS_FLT_MIN_EXP-bits;
		break;
	case 1:
		bits = FS_DBL_MANT_DIG;
		emin = FS_DBL_MIN_EXP-bits;
		break;
	case 2:
		bits = FS_MANT_DIG;
		emin = FS_MIN_EXP-bits;
		break;
	default:
		return 0;
	}

	while (isspace((c=shgetc(f))));

	if (c=='+' || c=='-') {
		sign -= 2*(c=='-');
		c = shgetc(f);
	}

	for (i=0; i<8 && (c|32)=="infinity"[i]; i++)
		if (i<7) c = shgetc(f);
	if (i==3 || i==8 || (i>3 && pok)) {
		if (i!=8) {
			shunget(f);
			if (pok) for (; i>3; i--) shunget(f);
		}
		return sign * __builtin_inf();
	}
	if (!i) for (i=0; i<3 && (c|32)=="nan"[i]; i++)
		if (i<2) c = shgetc(f);
	if (i==3) {
		if (shgetc(f) != '(') {
			shunget(f);
			return __builtin_nan("");
		}
		for (i=1; ; i++) {
			c = shgetc(f);
			if (c-'0'<10U || c-'A'<26U || c-'a'<26U || c=='_')
				continue;
			if (c==')') return __builtin_nan("");
			shunget(f);
			if (!pok) {
				errno = EINVAL;
				shlim(f, 0);
				return 0;
			}
			while (i--) shunget(f);
			return __builtin_nan("");
		}
		return __builtin_nan("");
	}

	if (i) {
		shunget(f);
		errno = EINVAL;
		shlim(f, 0);
		return 0;
	}

	if (c=='0') {
		c = shgetc(f);
		if ((c|32) == 'x')
			return hexfloat(f, bits, emin, sign, pok);
		shunget(f);
		c = '0';
	}

	return decfloat(f, c, bits, emin, sign, pok);
}

/* musl vfscanf.c arg_n: positional %n$ — walk a va_list copy to the n-th
 * pointer argument. Shared by the narrow vfscanf here and the wide vfwscanf
 * below (#660). Under the c2go void** tagged-pack va model each va_arg(void*)
 * consumes exactly one pack slot, so the walk is well-defined. */
static void *arg_n(va_list ap, unsigned int n)
{
	void *p;
	unsigned int i;
	va_list ap2;
	va_copy(ap2, ap);
	for (i=n; i>1; i--) va_arg(ap2, void *);
	p = va_arg(ap2, void *);
	va_end(ap2);
	return p;
}

c2go_extern
int vfscanf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	int width;
	int size;
	int base;
	const unsigned char *p;
	int c, t;
	int alloc = 0;
	char *s;
	wchar_t *wcs;
	mbstate_t st;
	void *dest=NULL;
	int invert;
	int matches=0;
	unsigned long long x;
	double y;
	off_t pos = 0;
	unsigned char scanset[257];
	size_t i, k;

	FLOCK(f);

	if (!f->rpos) __toread(f);
	if (!f->rpos) goto input_fail;

	for (p=(const unsigned char *)fmt; *p; p++) {

		if (isspace(*p)) {
			while (isspace(p[1])) p++;
			shlim(f, 0);
			while (isspace(shgetc(f)));
			shunget(f);
			pos += shcnt(f);
			continue;
		}
		if (*p != '%' || p[1] == '%') {
			shlim(f, 0);
			if (*p == '%') {
				p++;
				while (isspace((c=shgetc(f))));
			} else {
				c = shgetc(f);
			}
			if (c!=*p) {
				shunget(f);
				if (c<0) goto input_fail;
				goto match_fail;
			}
			pos += shcnt(f);
			continue;
		}

		p++;
		if (*p=='*') {
			dest = 0; p++;
		} else if (isdigit(*p) && p[1]=='$') {
			dest = arg_n(ap, *p-'0'); p+=2;
		} else {
			dest = va_arg(ap, void *);
		}

		for (width=0; isdigit(*p); p++) {
			width = 10*width + *p - '0';
		}

		if (*p=='m') {
			wcs = 0;
			s = 0;
			alloc = !!dest;
			p++;
		} else {
			alloc = 0;
		}

		size = SIZE_def;
		switch (*p++) {
		case 'h':
			if (*p == 'h') p++, size = SIZE_hh;
			else size = SIZE_h;
			break;
		case 'l':
			if (*p == 'l') p++, size = SIZE_ll;
			else size = SIZE_l;
			break;
		case 'j':
			size = SIZE_ll;
			break;
		case 'z':
		case 't':
#if defined(_WIN32)
			size = SIZE_ll; /* LLP64: size_t/ptrdiff_t are 8 bytes but long is 4 */
#else
			size = SIZE_l;
#endif
			break;
		case 'L':
			size = SIZE_L;
			break;
		case 'd': case 'i': case 'o': case 'u': case 'x':
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': case 'X':
		case 's': case 'c': case '[':
		case 'S': case 'C':
		case 'p': case 'n':
			p--;
			break;
		default:
			goto fmt_fail;
		}

		t = *p;

		/* C or S -> c or s; the wide (wchar_t) form is driven by size==SIZE_l
		 * in the scan branch below (musl vfscanf.c sets it here for %C/%S). */
		if ((t&0x2f) == 3) {
			t |= 32;
			size = SIZE_l;
		}

		switch (t) {
		case 'c':
			if (width < 1) width = 1;
		case '[':
			break;
		case 'n':
			store_int(dest, size, pos);
			/* do not increment match count, etc! */
			continue;
		default:
			shlim(f, 0);
			while (isspace(shgetc(f)));
			shunget(f);
			pos += shcnt(f);
		}

		shlim(f, width);
		if (shgetc(f) < 0) goto input_fail;
		shunget(f);

		switch (t) {
		case 's':
		case 'c':
		case '[':
			if (t == 'c' || t == 's') {
				memset(scanset, -1, sizeof scanset);
				scanset[0] = 0;
				if (t == 's') {
					scanset[1+'\t'] = 0;
					scanset[1+'\n'] = 0;
					scanset[1+'\v'] = 0;
					scanset[1+'\f'] = 0;
					scanset[1+'\r'] = 0;
					scanset[1+' '] = 0;
				}
			} else {
				if (*++p == '^') p++, invert = 1;
				else invert = 0;
				memset(scanset, invert, sizeof scanset);
				scanset[0] = 0;
				if (*p == '-') p++, scanset[1+'-'] = 1-invert;
				else if (*p == ']') p++, scanset[1+']'] = 1-invert;
				for (; *p != ']'; p++) {
					if (!*p) goto fmt_fail;
					if (*p=='-' && p[1] && p[1] != ']')
						for (c=p++[-1]; c<*p; c++)
							scanset[1+c] = 1-invert;
					scanset[1+*p] = 1-invert;
				}
			}
			wcs = 0;
			s = 0;
			i = 0;
			k = t=='c' ? width+1U : 31;
			if (size == SIZE_l) {
				/* %ls / %lc / %S / %C: decode input bytes into wchar_t via
				 * __mbrtoc32, ported from musl vfscanf.c. With %m (alloc) the
				 * result buffer is malloc'd and grown geometrically (#660);
				 * the mbstate_t carries a partial multibyte sequence across
				 * bytes. */
				if (alloc) {
					wcs = malloc(k*sizeof(wchar_t));
					if (!wcs) goto alloc_fail;
				} else {
					wcs = dest;
				}
				st = (mbstate_t){0};
				while (scanset[(c=shgetc(f))+1]) {
					unsigned sc;
					switch (__mbrtoc32(&sc, &(char){c}, 1, &st)) {
					case -1:  /* (size_t)-1: illegal sequence */
						goto input_fail;
					case -2:  /* (size_t)-2: incomplete, need the next byte */
						continue;
					}
					/* On Windows a supplementary scalar becomes a UTF-16 surrogate
					 * PAIR (two wchar_t units); one unit on the UTF-32 targets.
					 * __mbrtoc32 yields the full scalar (mbrtowc would EILSEQ it).
					 * The alloc-grow check runs BEFORE the append — a two-unit
					 * pair could overshoot musl's grow-after-one-unit check. */
					if (wcs) {
						if (alloc && i+2 >= k) {
							k += k+2;
							wchar_t *tmp = realloc(wcs, k*sizeof(wchar_t));
							if (!tmp) goto alloc_fail;
							wcs = tmp;
						}
						if (WCHAR_UTF16 && sc >= 0x10000u) {
							wcs[i++] = SURR_HIGH(sc);
							wcs[i++] = SURR_LOW(sc);
						} else {
							wcs[i++] = sc;
						}
					}
				}
				if (!mbsinit(&st)) goto input_fail;
			} else if (alloc) {
				s = malloc(k);
				if (!s) goto alloc_fail;
				while (scanset[(c=shgetc(f))+1]) {
					s[i++] = c;
					if (i==k) {
						k += k+1;
						char *tmp = realloc(s, k);
						if (!tmp) goto alloc_fail;
						s = tmp;
					}
				}
			} else if ((s = dest)) {
				while (scanset[(c=shgetc(f))+1])
					s[i++] = c;
			} else {
				while (scanset[(c=shgetc(f))+1]);
			}
			shunget(f);
			if (!shcnt(f)) goto match_fail;
			if (t == 'c' && shcnt(f) != width) goto match_fail;
			if (alloc) {
				if (size == SIZE_l) *(wchar_t **)dest = wcs;
				else *(char **)dest = s;
			}
			if (t != 'c') {
				if (wcs) wcs[i] = 0;
				if (s) s[i] = 0;
			}
			break;
		case 'p':
		case 'X':
		case 'x':
			base = 16;
			goto int_common;
		case 'o':
			base = 8;
			goto int_common;
		case 'd':
		case 'u':
			base = 10;
			goto int_common;
		case 'i':
			base = 0;
		int_common:
			x = __intscan(f, base, 0, ULLONG_MAX);
			if (!shcnt(f)) goto match_fail;
			if (t=='p' && dest) *(void **)dest = (void *)(uintptr_t)x;
			else store_int(dest, size, x);
			break;
		case 'a': case 'A':
		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
			y = __floatscan(f, size, 0);
			if (!shcnt(f)) goto match_fail;
			if (dest) switch (size) {
			case SIZE_def:
				*(float *)dest = y;
				break;
			case SIZE_l:
				*(double *)dest = y;
				break;
			case SIZE_L:
				/* long double == double in this port (arm64 repr). */
				*(double *)dest = y;
				break;
			}
			break;
		}

		pos += shcnt(f);
		if (dest) matches++;
	}
	if (0) {
fmt_fail:
alloc_fail:
input_fail:
		if (!matches) matches--;
match_fail:
		if (alloc) {
			free(s);
			free(wcs);
		}
	}
	FUNLOCK(f);
	return matches;
}

/* ── scanf family public wrappers (musl scanf.c/fscanf.c/…) ─────────────── */

c2go_extern int vscanf(const char *restrict fmt, va_list ap)
{
	return vfscanf(stdin, fmt, ap);
}

/* musl vsscanf.c string_read: feeds the pseudo-FILE from a NUL-terminated
 * string held in f->cookie. */
static size_t string_read(FILE *f, unsigned char *buf, size_t len)
{
	char *src = f->cookie;
	size_t k = len+256;
	char *end = memchr(src, 0, k);
	if (end) k = end-src;
	if (k < len) len = k;
	memcpy(buf, src, len);
	f->rpos = (void *)(src+len);
	f->rend = (void *)(src+k);
	f->cookie = src+k;
	return len;
}

c2go_extern int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap)
{
	FILE f;
	memset(&f, 0, sizeof f);
	f.buf = (void *)s;
	f.cookie = (void *)s;
	f.read = string_read;
	f.lock = -1;
	return vfscanf(&f, fmt, ap);
}

/* ── strtod / strtof / strtold / atof (musl src/stdlib) ──────────────────────
 * Co-located with __floatscan (static to this TU). musl's sh_fromstring turns the
 * NUL-terminated string into one unbounded scan buffer (rend = (void*)-1);
 * __floatscan halts at the first non-float byte, so __uflow / the read callback
 * are never reached. prec 0/1/2 = float/double/long double; long double == double
 * here (FS_MANT_DIG == FS_DBL_MANT_DIG == 53), so strtold matches strtod. */
static double strtox(const char *s, char **p, int prec)
{
	FILE f;
	memset(&f, 0, sizeof f);
	f.buf = f.rpos = (unsigned char *)s;
	f.rend = (unsigned char *)-1;
	f.lock = -1;
	shlim(&f, 0);
	double y = __floatscan(&f, prec, 1);
	off_t cnt = shcnt(&f);
	if (p) *p = cnt ? (char *)s + cnt : (char *)s;
	return y;
}

c2go_extern float strtof(const char *restrict s, char **restrict p)
{
	return (float)strtox(s, p, 0);
}

c2go_extern double strtod(const char *restrict s, char **restrict p)
{
	return strtox(s, p, 1);
}

c2go_extern long double strtold(const char *restrict s, char **restrict p)
{
	return strtox(s, p, 2);
}

c2go_extern double atof(const char *s)
{
	return strtod(s, 0);
}

/* ── strtol family (musl src/stdlib/strtol.c, #657) ──────────────────────────
 * Same string-fed stack FILE as the float strtox above, but through __intscan
 * — which is what wcstol already uses, so the narrow/wide parsers now agree.
 * vs the classic-BSD versions this replaces (stdlib.c): __intscan's pok
 * pushback leaves endptr after the '0' of an invalid "0x" prefix ("0xg"), and
 * it sets EINVAL for base == 1 or base > 36. */
static unsigned long long strtox_int(const char *s, char **p, int base, unsigned long long lim)
{
	FILE f;
	memset(&f, 0, sizeof f);
	f.buf = f.rpos = (unsigned char *)s;
	f.rend = (unsigned char *)-1;
	f.lock = -1;
	shlim(&f, 0);
	unsigned long long y = __intscan(&f, base, 1, lim);
	if (p) {
		off_t cnt = shcnt(&f);
		*p = (char *)s + cnt;
	}
	return y;
}

c2go_extern unsigned long long strtoull(const char *restrict s, char **restrict p, int base)
{
	return strtox_int(s, p, base, ULLONG_MAX);
}

c2go_extern long long strtoll(const char *restrict s, char **restrict p, int base)
{
	return strtox_int(s, p, base, LLONG_MIN);
}

c2go_extern unsigned long strtoul(const char *restrict s, char **restrict p, int base)
{
	return strtox_int(s, p, base, ULONG_MAX);
}

c2go_extern long strtol(const char *restrict s, char **restrict p, int base)
{
	return strtox_int(s, p, base, 0UL+LONG_MIN);
}

/* ── wide numeric parsing (musl src/stdlib/wcstol.c + wcstod.c) ──────────────
 * Co-located with __intscan / __floatscan (static to this TU) so the wide
 * parsers reuse them. The wide string is fed through a tiny stack FILE whose
 * read callback (do_read) copies each wchar_t < 128 verbatim into the byte
 * buffer and maps everything else to '@' (an illegal digit that halts the
 * scan). do_read is shared by the int and float helpers — musl's two copies
 * are byte-identical. FILE setup mirrors c2go's strtox above but adds the read
 * callback + cookie (the wide cursor) so __uflow refills from the wide string. */
static size_t do_read(FILE *f, unsigned char *buf, size_t len)
{
	size_t i;
	const wchar_t *wcs = f->cookie;

	if (!wcs[0]) wcs = L"@";
	for (i=0; i < f->buf_size && wcs[i]; i++)
		f->buf[i] = wcs[i] < 128 ? wcs[i] : '@';
	f->rpos = f->buf;
	f->rend = f->buf + i;
	f->cookie = (void *)(wcs+i);

	if (i && len) {
		*buf = *f->rpos++;
		return 1;
	}
	return 0;
}

static unsigned long long wcstox_int(const wchar_t *s, wchar_t **p, int base, unsigned long long lim)
{
	wchar_t *t = (wchar_t *)s;
	unsigned char buf[64];
	FILE f;
	memset(&f, 0, sizeof f);
	f.flags = 0;
	f.rpos = f.rend = f.buf = buf + 4;
	f.buf_size = sizeof buf - 4;
	f.lock = -1;
	f.read = do_read;
	while (iswspace(*t)) t++;
	f.cookie = (void *)t;
	shlim(&f, 0);
	unsigned long long y = __intscan(&f, base, 1, lim);
	if (p) {
		size_t cnt = shcnt(&f);
		*p = cnt ? t + cnt : (wchar_t *)s;
	}
	return y;
}

static double wcstox_float(const wchar_t *s, wchar_t **p, int prec)
{
	wchar_t *t = (wchar_t *)s;
	unsigned char buf[64];
	FILE f;
	memset(&f, 0, sizeof f);
	f.flags = 0;
	f.rpos = f.rend = f.buf = buf + 4;
	f.buf_size = sizeof buf - 4;
	f.lock = -1;
	f.read = do_read;
	while (iswspace(*t)) t++;
	f.cookie = (void *)t;
	shlim(&f, 0);
	double y = __floatscan(&f, prec, 1);
	if (p) {
		size_t cnt = shcnt(&f);
		*p = cnt ? t + cnt : (wchar_t *)s;
	}
	return y;
}

c2go_extern float wcstof(const wchar_t *restrict s, wchar_t **restrict p)
{
	return (float)wcstox_float(s, p, 0);
}

c2go_extern double wcstod(const wchar_t *restrict s, wchar_t **restrict p)
{
	return wcstox_float(s, p, 1);
}

c2go_extern long double wcstold(const wchar_t *restrict s, wchar_t **restrict p)
{
	return wcstox_float(s, p, 2);
}

c2go_extern long wcstol(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstox_int(s, p, base, 0UL+LONG_MIN);
}

c2go_extern unsigned long wcstoul(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstox_int(s, p, base, ULONG_MAX);
}

c2go_extern long long wcstoll(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstox_int(s, p, base, LLONG_MIN);
}

c2go_extern unsigned long long wcstoull(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstox_int(s, p, base, ULLONG_MAX);
}

c2go_extern intmax_t wcstoimax(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstoll(s, p, base);
}

c2go_extern uintmax_t wcstoumax(const wchar_t *restrict s, wchar_t **restrict p, int base)
{
	return wcstoull(s, p, base);
}

c2go_extern
int scanf(const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vscanf(fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int fscanf(FILE *restrict f, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfscanf(f, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int sscanf(const char *restrict s, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsscanf(s, fmt, ap);
	va_end(ap);
	return ret;
}

/* ─────────────────────────────────────────────────────────────────────────
 * FILE read path (#604/#605/#606): buffered character/line/block input, file
 * positioning, and stream status. Ported from musl's src/stdio/.
 *
 * Locking note (see the file header): these public entry points take the lock
 * once and use getc_unlocked inside that scope. This avoids redundant Go bridge
 * calls even though the shared per-FILE lock is recursive.
 * ───────────────────────────────────────────────────────────────────────── */

/* musl's getc_unlocked macro (stdio_impl.h): fast path when the read buffer is
 * non-empty, else refill via __uflow (defined above). */
#define getc_unlocked(f) \
	( ((f)->rpos != (f)->rend) ? *(f)->rpos++ : __uflow(f) )

c2go_extern
int fgetc(FILE *f)
{
	int c;
	FLOCK(f);
	c = getc_unlocked(f);
	FUNLOCK(f);
	return c;
}

c2go_extern
int getc(FILE *f)
{
	int c;
	FLOCK(f);
	c = getc_unlocked(f);
	FUNLOCK(f);
	return c;
}

c2go_extern
int getchar(void)
{
	int c;
	FLOCK(stdin);
	c = getc_unlocked(stdin);
	FUNLOCK(stdin);
	return c;
}

/* musl src/stdio/ungetc.c — verbatim. The UNGET-byte headroom before f->buf
 * (allocated by __fdopen / the static stdin) backs the push-back. */
c2go_extern
int ungetc(int c, FILE *f)
{
	if (c == EOF) return c;

	FLOCK(f);

	if (!f->rpos) __toread(f);
	if (!f->rpos || f->rpos <= f->buf - UNGET) {
		FUNLOCK(f);
		return EOF;
	}

	*--f->rpos = c;
	f->flags &= ~F_EOF;

	FUNLOCK(f);
	return (unsigned char)c;
}

/* musl src/stdio/fgets.c — verbatim (getc_unlocked + the feof MACRO, so no
 * re-lock inside the single FLOCK region). */
c2go_extern
char *fgets(char *restrict s, int n, FILE *restrict f)
{
	char *p = s;
	unsigned char *z;
	size_t k;
	int c;

	FLOCK(f);

	if (n<=1) {
		f->mode |= f->mode-1;
		FUNLOCK(f);
		if (n<1) return 0;
		*s = 0;
		return s;
	}
	n--;

	while (n) {
		if (f->rpos != f->rend) {
			z = memchr(f->rpos, '\n', f->rend - f->rpos);
			k = z ? (size_t)(z - f->rpos + 1) : (size_t)(f->rend - f->rpos);
			k = MIN(k, (size_t)n);
			memcpy(p, f->rpos, k);
			f->rpos += k;
			p += k;
			n -= k;
			if (z || !n) break;
		}
		if ((c = getc_unlocked(f)) < 0) {
			if (p==s || !feof(f)) s = 0;
			break;
		}
		n--;
		if ((*p++ = c) == '\n') break;
	}
	if (s) *p = 0;

	FUNLOCK(f);

	return s;
}

/* musl src/stdio/fread.c — verbatim. */
c2go_extern
size_t fread(void *restrict destv, size_t size, size_t nmemb, FILE *restrict f)
{
	unsigned char *dest = destv;
	size_t len = size*nmemb, l = len, k;
	if (!size) nmemb = 0;

	FLOCK(f);

	f->mode |= f->mode-1;

	if (f->rpos != f->rend) {
		/* First exhaust the buffer. */
		k = MIN((size_t)(f->rend - f->rpos), l);
		memcpy(dest, f->rpos, k);
		f->rpos += k;
		dest += k;
		l -= k;
	}

	/* Read the remainder directly. */
	for (; l; l-=k, dest+=k) {
		k = __toread(f) ? 0 : f->read(f, dest, l);
		if (!k) {
			FUNLOCK(f);
			return (len-l)/size;
		}
	}

	FUNLOCK(f);
	return nmemb;
}

/* ── getline/getdelim (#607) — musl src/stdio/{getdelim,getline}.c, verbatim.
 * Grows *s via realloc; uses getc_unlocked + the feof MACRO under one FLOCK. */
c2go_extern
ssize_t getdelim(char **restrict s, size_t *restrict n, int delim, FILE *restrict f)
{
	char *tmp;
	unsigned char *z;
	size_t k;
	size_t i=0;
	int c;

	FLOCK(f);

	if (!n || !s) {
		f->mode |= f->mode-1;
		f->flags |= F_ERR;
		FUNLOCK(f);
		errno = EINVAL;
		return -1;
	}

	if (!*s) *n=0;

	for (;;) {
		if (f->rpos != f->rend) {
			z = memchr(f->rpos, delim, f->rend - f->rpos);
			k = z ? (size_t)(z - f->rpos + 1) : (size_t)(f->rend - f->rpos);
		} else {
			z = 0;
			k = 0;
		}
		if (i+k >= *n) {
			size_t m = i+k+2;
			if (!z && m < SIZE_MAX/4) m += m/2;
			tmp = realloc(*s, m);
			if (!tmp) {
				m = i+k+2;
				tmp = realloc(*s, m);
				if (!tmp) {
					/* Copy as much as fits and ensure no
					 * pushback remains in the FILE buf. */
					k = *n-i;
					memcpy(*s+i, f->rpos, k);
					f->rpos += k;
					f->mode |= f->mode-1;
					f->flags |= F_ERR;
					FUNLOCK(f);
					errno = ENOMEM;
					return -1;
				}
			}
			*s = tmp;
			*n = m;
		}
		if (k) {
			memcpy(*s+i, f->rpos, k);
			f->rpos += k;
			i += k;
		}
		if (z) break;
		if ((c = getc_unlocked(f)) == EOF) {
			if (!i || !feof(f)) {
				FUNLOCK(f);
				return -1;
			}
			break;
		}
		/* If the byte read by getc won't fit without growing the
		 * output buffer, push it back for next iteration. */
		if (i+1 >= *n) *--f->rpos = c;
		else if (((*s)[i++] = c) == delim) break;
	}
	(*s)[i] = 0;

	FUNLOCK(f);

	return i;
}

c2go_extern
ssize_t getline(char **restrict s, size_t *restrict n, FILE *restrict f)
{
	return getdelim(s, n, '\n', f);
}

/* ── file positioning (#605) — musl src/stdio/{fseek,ftell,rewind,fgetpos,
 * fsetpos}.c. Built on f->seek (__stdio_seek) with musl's buffer accounting. */

static int __fseeko_unlocked(FILE *f, off_t off, int whence)
{
	/* Fail immediately for invalid whence. */
	if (whence != SEEK_CUR && whence != SEEK_SET && whence != SEEK_END) {
		errno = EINVAL;
		return -1;
	}
	/* Adjust relative offset for unread data in buffer, if any. */
	if (whence == SEEK_CUR && f->rend) off -= f->rend - f->rpos;
	/* Flush write buffer, and report error on failure. */
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return -1;
	}
	/* Leave writing mode. */
	f->wpos = f->wbase = f->wend = 0;
	/* Perform the underlying seek. */
	if (f->seek(f, off, whence) < 0) return -1;
	/* Seek succeeded: file is seekable, so discard the read buffer. */
	f->rpos = f->rend = 0;
	f->flags &= ~F_EOF;
	return 0;
}

static int __fseeko(FILE *f, off_t off, int whence)
{
	int result;
	FLOCK(f);
	result = __fseeko_unlocked(f, off, whence);
	FUNLOCK(f);
	return result;
}

c2go_extern
int fseek(FILE *f, long off, int whence)
{
	return __fseeko(f, off, whence);
}

c2go_extern
int fseeko(FILE *f, off_t off, int whence)
{
	return __fseeko(f, off, whence);
}

static off_t __ftello_unlocked(FILE *f)
{
	off_t pos = f->seek(f, 0,
		(f->flags & F_APP) && f->wpos != f->wbase
		? SEEK_END : SEEK_CUR);
	if (pos < 0) return pos;
	/* Adjust for data in buffer. */
	if (f->rend)
		pos += f->rpos - f->rend;
	else if (f->wbase)
		pos += f->wpos - f->wbase;
	return pos;
}

static off_t __ftello(FILE *f)
{
	off_t pos;
	FLOCK(f);
	pos = __ftello_unlocked(f);
	FUNLOCK(f);
	return pos;
}

c2go_extern
long ftell(FILE *f)
{
	off_t pos = __ftello(f);
	if (pos > LONG_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	return pos;
}

c2go_extern
off_t ftello(FILE *f)
{
	return __ftello(f);
}

c2go_extern
void rewind(FILE *f)
{
	FLOCK(f);
	__fseeko_unlocked(f, 0, SEEK_SET);
	f->flags &= ~F_ERR;
	FUNLOCK(f);
}

/* musl src/stdio/{fgetpos,fsetpos}.c. fpos_t is a 64-bit stream position. */
c2go_extern
int fgetpos(FILE *restrict f, fpos_t *restrict pos)
{
	off_t off = __ftello(f);
	if (off < 0) return -1;
	*pos = off;
	return 0;
}

c2go_extern
int fsetpos(FILE *f, const fpos_t *pos)
{
	return __fseeko(f, *pos, SEEK_SET);
}

/* ── buffering mode (#608) — musl src/stdio/{setvbuf,setbuf}.c, verbatim.
 * Defined behaviour only as the first operation on a fresh stream, so no lock.
 * No allocation: a caller buffer is used if suitably sized; _IONBF sets
 * buf_size=0 so __towrite makes wend==wpos and every putc overflows/flushes.
 * The FILE+buffer live in one malloc block (__fdopen), so fclose frees the FILE
 * regardless of a redirected f->buf — a caller setvbuf buffer is never freed. */
c2go_extern
int setvbuf(FILE *restrict f, char *restrict buf, int mode, size_t size)
{
	/* musl names this parameter `type`; renamed `mode` because c2go-bind does
	 * not sanitise Go reserved words in parameter names (a `type` param yields
	 * invalid Go). Behaviour is identical. */
	f->lbf = EOF;

	if (mode == _IONBF) {
		f->buf_size = 0;
	} else if (mode == _IOLBF || mode == _IOFBF) {
		if (buf && size >= UNGET) {
			f->buf = (void *)(buf + UNGET);
			f->buf_size = size - UNGET;
		}
		if (mode == _IOLBF && f->buf_size)
			f->lbf = '\n';
	} else {
		return -1;
	}

	f->flags |= F_SVB;

	return 0;
}

c2go_extern
void setbuf(FILE *restrict f, char *restrict buf)
{
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

/* ── stream status (#606) — musl src/stdio/{feof,ferror,clearerr}.c.
 * The public functions need the real symbols, so #undef the internal fast-path
 * macros first (mirroring musl's per-file #undef). No internal code past this
 * point uses the macro form (the last macro use is __fwritex/vfprintf, above). */
#undef feof
#undef ferror

c2go_extern
int feof(FILE *f)
{
	int ret;
	FLOCK(f);
	ret = !!(f->flags & F_EOF);
	FUNLOCK(f);
	return ret;
}

c2go_extern
int ferror(FILE *f)
{
	int ret;
	FLOCK(f);
	ret = !!(f->flags & F_ERR);
	FUNLOCK(f);
	return ret;
}

c2go_extern
void clearerr(FILE *f)
{
	FLOCK(f);
	f->flags &= ~(F_EOF|F_ERR);
	FUNLOCK(f);
}

/* musl fileno.c: the stream's underlying fd, or EBADF for a stream with none.
 * Std streams need NO translation: 0/1/2 are virtualized at the fd layer, so
 * the literal number IS the honest answer — write(fileno(stdout),...) routes
 * to the same live sink printf does. */
c2go_extern
int fileno(FILE *f)
{
	FLOCK(f);
	int fd = f->fd;
	FUNLOCK(f);
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
	return fd;
}

/* ── remove / tmpfile ───────────────────────────────────────────────────────
 * These are the stdio-level file operations layered on the per-OS unlink/rmdir
 * primitives (<unistd.h>) and rename (bound directly; see <stdio.h>). */

/* remove: mirror Go's os.Remove (os/file_unix.go) rather than musl's
 * EISDIR-only check — try unlink, then rmdir, deciding the reported errno from
 * rmdir. musl's `unlink; if EISDIR rmdir` is Linux-only: macOS returns EPERM (not
 * EISDIR) for unlink(dir), so an errno test does not port. Instead try both:
 * rmdir(file) yields ENOTDIR on every OS, so when rmdir fails with something else
 * the path was directory-like and rmdir's error is the real one; otherwise keep
 * unlink's. On Windows unlink/rmdir are the msvcrt _unlink/_rmdir wrappers.
 * (remove is a clang LibFunc like fopen — same benign builtin-CC warning, no
 * no_builtin needed; see fopen.) */
c2go_extern
int remove(const char *path)
{
	int e;
	if (unlink(path) == 0) return 0;
	e = errno;                       /* unlink's error */
	if (rmdir(path) == 0) return 0;
	if (errno == ENOTDIR) errno = e; /* not a dir: unlink's error is the real one */
	return -1;
}

/* __c2go_runtime_rand (stdio.go) surfaces the Go runtime PRNG. musl's __randname
 * seeds from clock_gettime + the thread id; this port has neither, so the
 * temp-name entropy reuses runtime.rand (the source os.CreateTemp uses). This is
 * the ONLY Go primitive the Unix tmpfile reaches for beyond the fd layer — the
 * retry loop / name build / unlink / __fdopen are all C, mirroring musl. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_runtime_rand", C2GO_GOABI0)
unsigned long long __c2go_runtime_rand(void);

/* musl src/temp/__randname.c: fill 6 template chars from one random word (the
 * char mapping is musl's; only the entropy source is swapped, per above). */
static char *randname(char *tmpl)
{
	int i;
	unsigned long long r = __c2go_runtime_rand();
	for (i = 0; i < 6; i++, r >>= 5)
		tmpl[i] = 'A' + (r & 15) + (r & 16) * 2;
	return tmpl;
}

/* tmpfile, musl src/stdio/tmpfile.c shape with ONE cross-world deviation
 * (#651): the temp DIRECTORY comes from Go's os.TempDir instead of musl's
 * hard-coded /tmp — on unix that honours $TMPDIR (falling back to /tmp), so
 * C's tmpfile and the Go host's os.CreateTemp land in the same directory; on
 * Windows it is the CRT-shaped TMP/TEMP resolution (no /tmp exists). The
 * O_EXCL retry loop, name build and __fdopen stay C; the random word comes
 * from __c2go_runtime_rand. Anonymity is per-OS: unix unlinks the open file;
 * Windows cannot (no unlink-while-open), so it opens with O_TEMPORARY (msvcrt
 * _O_TEMPORARY — the CRT deletes on last close). (tmpfile is a clang LibFunc
 * like fopen — same benign builtin-CC warning, no no_builtin needed.) */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_tmpdir", C2GO_GOABI0)
int __c2go_tmpdir(char *buf, int n);

c2go_extern
FILE *tmpfile(void)
{
	char s[300];
	int dl = __c2go_tmpdir(s, (int)sizeof s - 20); /* leave room for the name */
	if (dl < 0) return 0;
	static const char suf[] = "/tmpfile_XXXXXX";    /* msvcrt _open accepts '/' */
	int i;
	for (i = 0; suf[i]; i++) s[dl + i] = suf[i];
	s[dl + i] = 0;
	char *name = s + dl + i - 6;                     /* the XXXXXX */
	int fd, try;
	FILE *f;
	for (try = 0; try < 100; try++) {
		randname(name);
#if defined(_WIN32)
		fd = open(s, O_RDWR | O_CREAT | O_EXCL | O_TEMPORARY, 0600);
		if (fd >= 0) {
			f = __fdopen(fd, "w+");
			if (!f) close(fd);
			return f;
		}
#else
		fd = open(s, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			unlink(s);   /* anonymous: the fd is the only reference */
			f = __fdopen(fd, "w+");
			if (!f) close(fd);
			return f;
		}
#endif
	}
	return 0;
}

/* musl src/temp/mkstemp.c (via __mkostemps): the template must end in "XXXXXX";
 * fill those 6 with randname and open O_RDWR|O_CREAT|O_EXCL until one wins (or 100
 * EEXIST retries fail). Cross-platform: only the caller-supplied path + open() are
 * used (no /tmp assumption), and open() exists on both OSes. On give-up the
 * template's XXXXXX is restored and errno is left as open() set it. */
c2go_extern
int mkstemp(char *template)
{
	size_t l = strlen(template);
	if (l < 6 || memcmp(template + l - 6, "XXXXXX", 6)) {
		errno = EINVAL;
		return -1;
	}
	int fd, retries = 100;
	do {
		randname(template + l - 6);
		if ((fd = open(template, O_RDWR | O_CREAT | O_EXCL, 0600)) >= 0)
			return fd;
	} while (--retries && errno == EEXIST);
	memcpy(template + l - 6, "XXXXXX", 6);
	return -1;
}

/* musl src/temp/mkstemps.c (via __mkostemps): like mkstemp, but the "XXXXXX"
 * sits suffixlen bytes before the end. Same cross-platform footing. */
c2go_extern
int mkstemps(char *template, int suffixlen)
{
	size_t l = strlen(template);
	if (l < 6 || suffixlen < 0 || (size_t)suffixlen > l - 6 ||
	    memcmp(template + l - suffixlen - 6, "XXXXXX", 6)) {
		errno = EINVAL;
		return -1;
	}
	int fd, retries = 100;
	do {
		randname(template + l - suffixlen - 6);
		if ((fd = open(template, O_RDWR | O_CREAT | O_EXCL, 0600)) >= 0)
			return fd;
	} while (--retries && errno == EEXIST);
	memcpy(template + l - suffixlen - 6, "XXXXXX", 6);
	return -1;
}

/* musl src/temp/mkdtemp.c: same template contract, mkdir 0700 loop. mkdir is
 * cross-platform (stat layer), so mkdtemp is too. */
c2go_extern
char *mkdtemp(char *template)
{
	size_t l = strlen(template);
	if (l < 6 || memcmp(template + l - 6, "XXXXXX", 6)) {
		errno = EINVAL;
		return 0;
	}
	int retries = 100;
	do {
		randname(template + l - 6);
		if (!mkdir(template, 0700))
			return template;
	} while (--retries && errno == EEXIST);
	memcpy(template + l - 6, "XXXXXX", 6);
	return 0;
}

/* creat(path, mode) is the historical shorthand for open with the create /
 * write-only / truncate flag triple (POSIX defines it exactly so). open() is
 * per-OS (io_posix.c / io_windows.c) but the wrapper is data-model-neutral, so
 * it lives here next to mkstemp rather than being duplicated in both. */
c2go_extern
int creat(const char *path, mode_t mode)
{
	return open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

/* ── wide character FILE I/O (musl src/stdio: fwide/fputwc/fgetwc/...) ────────
 * Ported from musl with its per-thread locale switching removed: c2go-libc is
 * UTF-8-only (MB_CUR_MAX is a constant 4), so wctomb/mbtowc/mbrtowc/wcrtomb
 * already run in UTF-8 unconditionally and the FILE's `locale` field is unused.
 * The FILE `mode` field is the byte(<0)/wide(>0) orientation. The internal
 * __f{put,get}wc_unlocked helpers are file-local static (they must live in this
 * TU to reach the static __fwritex / putc_unlocked / getc_unlocked / __toread /
 * ungetc). wchar_t is int32 (UTF-32) on the unix targets; on Windows it is
 * uint16 and supplementary scalars travel as surrogate PAIRS through
 * fputwc/fgetwc/ungetwc (the FILE wpend slot buffers the half in flight).
 * musl's weak_alias'd *_unlocked public variants are omitted.
 *
 * MB_LEN_MAX comes from <limits.h> (included above). It must NOT be re-pinned
 * `#ifndef`-style here: that fallback silently masked the windows-goabi case
 * where clang's resource <limits.h> won the include search and defined
 * MB_LEN_MAX as 1, shrinking every conversion buffer below (wcrtomb/wctomb
 * write up to 4 bytes — stack corruption of whatever followed the buffer). */

c2go_extern
int fwide(FILE *f, int mode)
{
	FLOCK(f);
	if (mode) {
		if (!f->mode) f->mode = mode>0 ? 1 : -1;
	}
	mode = f->mode;
	FUNLOCK(f);
	return mode;
}

/* Set wide orientation WITHOUT taking the FILE lock: the callers below already
 * hold it, and the c2go FILE lock is NOT reentrant (unlike musl's __lockfile,
 * which no-ops a same-thread re-lock), so calling the public fwide() from a
 * locked region self-deadlocks. Equivalent to fwide(f,1): fwide only ever acts
 * when f->mode is 0 (an orientation, once set, is sticky). */
static void __fwide_wide(FILE *f)
{
	if (!f->mode) f->mode = 1;
}

/* Non-locking ungetc core (mirrors ungetc()'s body after its FLOCK) for use
 * inside an already-locked wide read — the public ungetc would re-lock and, with
 * the non-reentrant FILE lock, deadlock. */
static int __ungetc_unlocked(int c, FILE *f)
{
	if (c == EOF) return c;
	if (!f->rpos) __toread(f);
	if (!f->rpos || f->rpos <= f->buf - UNGET) return EOF;
	*--f->rpos = c;
	f->flags &= ~F_EOF;
	return (unsigned char)c;
}

static wint_t __fputwc_unlocked(wchar_t wc, FILE *f)
{
	char mbc[MB_LEN_MAX];
	int l;
	wint_t c = wc; /* WEOF must not truncate through 16-bit wchar_t on Windows. */

	__fwide_wide(f);

	if (WCHAR_UTF16) {
		/* UTF-16 (Windows): a supplementary scalar arrives as two fputwc calls —
		 * a high surrogate then a low. Buffer the high (f->wpend) and emit the
		 * 4-byte UTF-8 form when the low arrives (wcsrtombs combines the pair). A
		 * lone/unpaired surrogate is an EILSEQ. */
		if (f->wpend) {
			if (IS_LOW_SURR(c)) {
				/* Combine into the supplementary scalar and emit its 4-byte UTF-8
				 * form, like the musl non-ASCII path below (stack buffer through
				 * __fwritex). */
				unsigned cp = 0x10000u + ((f->wpend - 0xd800u) << 10) + (c - 0xdc00u);
				f->wpend = 0;
				mbc[0] = 0xf0 | (cp >> 18);
				mbc[1] = 0x80 | ((cp >> 12) & 0x3f);
				mbc[2] = 0x80 | ((cp >> 6) & 0x3f);
				mbc[3] = 0x80 | (cp & 0x3f);
				if (__fwritex((void *)mbc, 4, f) < 4) {
					f->flags |= F_ERR;
					return WEOF;
				}
				return c;
			}
			f->wpend = 0;               /* buffered high never got its low */
			errno = EILSEQ;
			f->flags |= F_ERR;
			return WEOF;
		}
		if (IS_HIGH_SURR(c)) { f->wpend = (unsigned)c; return c; }  /* hold for its low */
		if (IS_LOW_SURR(c)) { errno = EILSEQ; f->flags |= F_ERR; return WEOF; }
	}

	if (isascii(c)) {
		c = putc_unlocked(c, f);
	} else if (f->wpos + MB_LEN_MAX < f->wend) {
		l = wctomb((void *)f->wpos, c);
		if (l < 0) c = WEOF;
		else f->wpos += l;
	} else {
		l = wctomb(mbc, c);
		if (l < 0 || __fwritex((void *)mbc, l, f) < l) c = WEOF;
	}
	if (c==WEOF) f->flags |= F_ERR;
	return c;
}

c2go_extern
wint_t fputwc(wchar_t c, FILE *f)
{
	wint_t r;
	FLOCK(f);
	r = __fputwc_unlocked(c, f);
	FUNLOCK(f);
	return r;
}

c2go_extern
wint_t putwc(wchar_t c, FILE *f)
{
	return fputwc(c, f);
}

c2go_extern
wint_t putwchar(wchar_t c)
{
	return fputwc(c, stdout);
}

/* Split a decoded scalar into what fgetwc returns now, parking the low surrogate
 * of a supplementary pair (UTF-16 / Windows) for the next call. */
static wint_t __fgetwc_emit(FILE *f, unsigned scalar)
{
	if (WCHAR_UTF16 && scalar >= 0x10000) {
		f->wpend = SURR_LOW(scalar);
		return SURR_HIGH(scalar);
	}
	return scalar;
}

static wint_t __fgetwc_unlocked_internal(FILE *f)
{
	unsigned scalar;
	int c;
	size_t l;

	/* A low surrogate parked by the previous supplementary read (or an
	 * ungetwc'd surrogate) is returned first. */
	if (WCHAR_UTF16 && f->wpend) {
		wint_t r = f->wpend;
		f->wpend = 0;
		return r;
	}

	/* Convert a character straight from the read buffer if a whole one is there.
	 * __mbrtoc32 yields the 32-bit scalar (mbtowc would report EILSEQ for a
	 * supplementary scalar on UTF-16); a fresh state makes it a one-shot decode. */
	if (f->rpos != f->rend) {
		mbstate_t fast = { 0 };
		l = __mbrtoc32(&scalar, (void *)f->rpos, f->rend - f->rpos, &fast);
		if (l <= 4) {   /* a byte count 0..4; -1/-2 are the huge size_t errors */
			f->rpos += l + !l; /* l==0 means a 1-byte NUL */
			return __fgetwc_emit(f, scalar);
		}
	}

	/* Otherwise assemble it byte-by-byte across the buffer boundary. */
	mbstate_t st = { 0 };
	unsigned char b;
	int first = 1;
	do {
		b = c = getc_unlocked(f);
		if (c < 0) {
			if (!first) {
				f->flags |= F_ERR;
				errno = EILSEQ;
			}
			return WEOF;
		}
		l = __mbrtoc32(&scalar, (void *)&b, 1, &st);
		if (l == -1) {
			if (!first) {
				f->flags |= F_ERR;
				__ungetc_unlocked(b, f);
			}
			return WEOF;
		}
		first = 0;
	} while (l == -2);

	return __fgetwc_emit(f, scalar);
}

static wint_t __fgetwc_unlocked(FILE *f)
{
	__fwide_wide(f);
	return __fgetwc_unlocked_internal(f);
}

c2go_extern
wint_t fgetwc(FILE *f)
{
	wint_t c;
	FLOCK(f);
	c = __fgetwc_unlocked(f);
	FUNLOCK(f);
	return c;
}

c2go_extern
wint_t getwc(FILE *f)
{
	return fgetwc(f);
}

c2go_extern
wint_t getwchar(void)
{
	return fgetwc(stdin);
}

c2go_extern
int fputws(const wchar_t *restrict ws, FILE *restrict f)
{
	unsigned char buf[BUFSIZ];
	size_t l=0;

	FLOCK(f);
	__fwide_wide(f);

	while (ws && (l = wcsrtombs((void *)buf, (void*)&ws, sizeof buf, 0))+1 > 1)
		if (__fwritex(buf, l, f) < l) {
			FUNLOCK(f);
			return -1;
		}

	FUNLOCK(f);
	return l; /* 0 or -1 */
}

c2go_extern
wchar_t *fgetws(wchar_t *restrict s, int n, FILE *restrict f)
{
	wchar_t *p = s;

	if (!n--) return s;

	FLOCK(f);

	for (; n; n--) {
		wint_t c = __fgetwc_unlocked(f);
		if (c == WEOF) break;
		*p++ = c;
		if (c == '\n') break;
	}
	*p = 0;
	if (f->flags & F_ERR) p = s; /* the ferror MACRO — no re-lock in the FLOCK region */

	FUNLOCK(f);

	return (p == s) ? NULL : s;
}

/* Non-locking ungetwc core (mirrors the public ungetwc()'s body after its
 * FLOCK). The wide-scanf reader below runs under vfwscanf's FLOCK and the c2go
 * FILE lock is NOT reentrant, so it cannot call the public (locking) ungetwc;
 * it uses this twin instead. */
static wint_t __ungetwc_unlocked(wint_t c, FILE *f)
{
	unsigned char mbc[MB_LEN_MAX];
	int l;

	__fwide_wide(f);

	if (WCHAR_UTF16 && (IS_HIGH_SURR(c) || IS_LOW_SURR(c))) {
		/* A surrogate half has no standalone byte encoding; push it into the same
		 * one-slot pending buffer fgetwc drains. Only one push may be pending. */
		if (f->wpend) return WEOF;
		if (!f->rpos) __toread(f);
		if (!f->rpos) return WEOF;
		f->wpend = (unsigned)c;
		f->flags &= ~F_EOF;
		return c;
	}

	if (!f->rpos) __toread(f);
	if (!f->rpos || c == WEOF || (l = wcrtomb((void *)mbc, c, 0)) < 0 ||
	    f->rpos < f->buf - UNGET + l) {
		return WEOF;
	}

	if (isascii(c)) *--f->rpos = c;
	else memcpy(f->rpos -= l, mbc, l);

	f->flags &= ~F_EOF;

	return c;
}

c2go_extern
wint_t ungetwc(wint_t c, FILE *f)
{
	FLOCK(f);
	wint_t r = __ungetwc_unlocked(c, f);
	FUNLOCK(f);
	return r;
}

/* ── wide formatted output (musl src/stdio/vfwprintf.c + vswprintf.c) ─────
 *
 * Faithful port of musl's wprintf_core / vfwprintf / vswprintf and the
 * swprintf/fwprintf/wprintf/vwprintf wrappers. The shared printf machinery
 * (union arg, pop_arg, the states[] table, the flag macros, NL_ARGMAX) is
 * REUSED from the narrow printf_core above — this is the same translation unit,
 * so musl's wide core drives the identical enum/table.
 *
 * c2go adaptations (the ONLY deviations from musl):
 *   - The FILE lock is a plain NON-reentrant mutex (unlike musl's recursive
 *     __lockfile), so inside vfwprintf's FLOCK region we must not call any
 *     PUBLIC locking function. musl's out()/pad()/fprintf() are therefore
 *     replaced by non-locking twins: wout() drives __fputwc_unlocked, wpad()
 *     and the numeric conversions go through fwprintf_num() which calls the
 *     UNLOCKED printf_core directly. (ASCII numeric output is single-byte wide
 *     chars, so writing those bytes into the wide stream is correct.)
 *   - fwide(f,1) -> __fwide_wide(f) (non-locking, defined by the 7b-1 block).
 *   - musl's non-locking ferror(f) MACRO was #undef'd earlier in this TU (before
 *     the public LOCKING ferror() was defined), and this code sits past that
 *     point, so error state is tested as `f->flags & F_ERR` directly.
 *   - Per-thread locale switching is dropped (c2go is UTF-8-only). The
 *     ASCII-digit test for %n$ / field-width / precision parsing is the real
 *     iswdigit from <wctype.h> (a pure leaf, safe to call inside the FLOCK). */

/* wide out — writes wchar_t; MUST use __fputwc_unlocked (we run under
 * vfwprintf's FLOCK and the c2go FILE lock is NOT reentrant — the public
 * fputwc would self-deadlock). */
static void wout(FILE *f, const wchar_t *s, size_t l)
{
	while (l-- && !(f->flags & F_ERR)) __fputwc_unlocked(*s++, f);
}

/* wide getint — musl's getint but over wchar_t*. */
static int wgetint(wchar_t **s) {
	int i;
	for (i=0; iswdigit(**s); (*s)++) {
		if (i > INT_MAX/10U || **s-'0' > INT_MAX-10*i) i = -1;
		else i = 10*i + (**s-'0');
	}
	return i;
}

/* Non-locking byte fprintf(f, ...) for the numeric conversions: calls the
 * UNLOCKED printf_core directly (the public fprintf/vfprintf would FLOCK and
 * deadlock under vfwprintf's lock). ASCII numeric output == single-byte wide
 * chars, so writing the bytes straight into f is correct on a wide stream. */
static int fwprintf_num(FILE *f, const char *charfmt, ...)
{
	va_list ap;
	int nlt[NL_ARGMAX+1] = {0};
	union arg nla[NL_ARGMAX+1];
	va_start(ap, charfmt);
	int r = printf_core(f, charfmt, &ap, nla, nlt);
	va_end(ap);
	return r;
}

/* wide pad — musl's pad(f,n,fl) but via the non-locking fwprintf_num. */
static void wpad(FILE *f, int n, int fl)
{
	if ((fl & LEFT_ADJ) || !n || (f->flags & F_ERR)) return;
	fwprintf_num(f, "%*s", n, "");
}

static const char wsizeprefix['y'-'a'] = {
['a'-'a']='L', ['e'-'a']='L', ['f'-'a']='L', ['g'-'a']='L',
['d'-'a']='j', ['i'-'a']='j', ['o'-'a']='j', ['u'-'a']='j', ['x'-'a']='j',
['p'-'a']='j'
};

static int wprintf_core(FILE *f, const wchar_t *fmt, va_list *ap, union arg *nl_arg, int *nl_type)
{
	wchar_t *a, *z, *s=(wchar_t *)fmt;
	unsigned l10n=0, fl;
	int w, p, xp;
	union arg arg;
	int argpos;
	unsigned st, ps;
	int cnt=0, l=0;
	int i;
	int t;
	char *bs;
	char charfmt[16];
	unsigned cp;
	size_t mr;

	for (;;) {
		/* This error is only specified for snprintf, but since it's
		 * unspecified for other forms, do the same. Stop immediately
		 * on overflow; otherwise %n could produce wrong results. */
		if (l > INT_MAX - cnt) goto overflow;

		/* Update output count, end loop when fmt is exhausted */
		cnt += l;
		if (!*s) break;

		/* Handle literal text and %% format specifiers */
		for (a=s; *s && *s!='%'; s++);
		for (z=s; s[0]=='%' && s[1]=='%'; z++, s+=2);
		if (z-a > INT_MAX-cnt) goto overflow;
		l = z-a;
		if (f) wout(f, a, l);
		if (l) continue;

		if (iswdigit(s[1]) && s[2]=='$') {
			l10n=1;
			argpos = s[1]-'0';
			s+=3;
		} else {
			argpos = -1;
			s++;
		}

		/* Read modifier flags */
		for (fl=0; (unsigned)*s-' '<32 && (FLAGMASK&(1U<<*s-' ')); s++)
			fl |= 1U<<*s-' ';

		/* Read field width */
		if (*s=='*') {
			if (iswdigit(s[1]) && s[2]=='$') {
				l10n=1;
				nl_type[s[1]-'0'] = INT;
				w = nl_arg[s[1]-'0'].i;
				s+=3;
			} else if (!l10n) {
				w = f ? va_arg(*ap, int) : 0;
				s++;
			} else goto inval;
			if (w<0) fl|=LEFT_ADJ, w=-w;
		} else if ((w=wgetint(&s))<0) goto overflow;

		/* Read precision */
		if (*s=='.' && s[1]=='*') {
			if (isdigit(s[2]) && s[3]=='$') {
				nl_type[s[2]-'0'] = INT;
				p = nl_arg[s[2]-'0'].i;
				s+=4;
			} else if (!l10n) {
				p = f ? va_arg(*ap, int) : 0;
				s+=2;
			} else goto inval;
			xp = (p>=0);
		} else if (*s=='.') {
			s++;
			p = wgetint(&s);
			xp = 1;
		} else {
			p = -1;
			xp = 0;
		}

		/* Format specifier state machine */
		st=0;
		do {
			if (OOB(*s)) goto inval;
			ps=st;
			st=states[st]S(*s++);
		} while (st-1<STOP);
		if (!st) goto inval;

		/* Check validity of argument type (nl/normal) */
		if (st==NOARG) {
			if (argpos>=0) goto inval;
		} else {
			if (argpos>=0) nl_type[argpos]=st, arg=nl_arg[argpos];
			else if (f) pop_arg(&arg, st, ap);
			else return 0;
		}

		if (!f) continue;

		/* Do not process any new directives once in error state. musl uses the
		 * non-locking ferror(f) MACRO here, but this TU #undef'd it before the
		 * public ferror() (a LOCKING function) was defined, and this code sits
		 * past that point — so test the flag directly to avoid re-locking the
		 * non-reentrant FILE lock we already hold. */
		if (f->flags & F_ERR) return -1;

		t = s[-1];
		if (ps && (t&15)==3) t&=~32;

		switch (t) {
		case 'n':
			switch(ps) {
			case BARE: *(int *)arg.p = cnt; break;
			case LPRE: *(long *)arg.p = cnt; break;
			case LLPRE: *(long long *)arg.p = cnt; break;
			case HPRE: *(unsigned short *)arg.p = cnt; break;
			case HHPRE: *(unsigned char *)arg.p = cnt; break;
			case ZTPRE: *(size_t *)arg.p = cnt; break;
			case JPRE: *(uintmax_t *)arg.p = cnt; break;
			}
			continue;
		case 'c':
		case 'C':
			if (w<1) w=1;
			wpad(f, w-1, fl);
			wout(f, &(wchar_t){t=='C' ? arg.i : btowc(arg.i)}, 1);
			wpad(f, w-1, fl^LEFT_ADJ);
			l = w;
			continue;
		case 'S':
			a = arg.p;
			z = a + wcsnlen(a, p<0 ? INT_MAX : p);
			if (p<0 && *z) goto overflow;
			p = z-a;
			/* Don't split a surrogate pair at the precision boundary: a trailing
			 * high surrogate whose low was truncated away would be emitted alone
			 * and park in the wide stream's pending-surrogate slot (wpend),
			 * poisoning subsequent output. Drop it — like %.Ns of a supplementary
			 * %s, which emits nothing rather than a lone half. */
			if (WCHAR_UTF16 && p>0 && IS_HIGH_SURR(a[p-1])) p--;
			if (w<p) w=p;
			wpad(f, w-p, fl);
			wout(f, a, p);
			wpad(f, w-p, fl^LEFT_ADJ);
			l=w;
			continue;
		case 'm':
			arg.p = strerror(errno);
		case 's':
			if (!arg.p) arg.p = "(null)";
			bs = arg.p;
			/* Measure: l = number of UTF-16 output UNITS. A supplementary scalar is
			 * two units on Windows, one on the UTF-32 targets; precision caps units
			 * and never splits a pair across the cap (so %.1s of a supplementary
			 * emits nothing, not a lone surrogate). __mbrtoc32 reaches the scalar
			 * that a per-unit mbtowc would EILSEQ on a 16-bit wchar_t. The `l<cap`
			 * guard short-circuits like musl's `l<p` so the byte at the precision
			 * boundary is never decoded (an illegal byte there must not fail the
			 * whole conversion — %.3s of "abc\xff" truncates cleanly to "abc"). */
			for (l=0; l < (p<0?INT_MAX:p); bs+=mr) {
				mbstate_t ms = {0};
				mr = __mbrtoc32(&cp, bs, MB_LEN_MAX, &ms);
				if (mr==(size_t)-1) return -1;
				if (mr==(size_t)-2 || mr==0) break;
				int units = (WCHAR_UTF16 && cp>=0x10000u) ? 2 : 1;
				if (l+units > (p<0?INT_MAX:p)) break;
				l += units;
			}
			if (p<0 && *bs) goto overflow;
			p=l;
			if (w<p) w=p;
			wpad(f, w-p, fl);
			bs = arg.p;
			/* Emit: re-decode the measured prefix. A supplementary is written as its
			 * high+low surrogate units on Windows (wout -> __fputwc_unlocked pairs
			 * them back into the 4-byte UTF-8 form); one unit on the UTF-32 targets. */
			while (l>0) {
				mbstate_t ms = {0};
				mr = __mbrtoc32(&cp, bs, MB_LEN_MAX, &ms);
				bs += mr;
				if (WCHAR_UTF16 && cp>=0x10000u) {
					wchar_t pr[2] = { SURR_HIGH(cp), SURR_LOW(cp) };
					wout(f, pr, 2);
					l -= 2;
				} else {
					wchar_t u = cp;
					wout(f, &u, 1);
					l -= 1;
				}
			}
			wpad(f, w-p, fl^LEFT_ADJ);
			l=w;
			continue;
		}

		if (xp && p<0) goto overflow;
		snprintf(charfmt, sizeof charfmt, "%%%s%s%s%s%s*.*%c%c",
			"#"+!(fl & ALT_FORM),
			"+"+!(fl & MARK_POS),
			"-"+!(fl & LEFT_ADJ),
			" "+!(fl & PAD_POS),
			"0"+!(fl & ZERO_PAD),
			wsizeprefix[(t|32)-'a'], t);

		switch (t|32) {
		case 'a': case 'e': case 'f': case 'g':
			l = fwprintf_num(f, charfmt, w, p, arg.f);
			break;
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'p':
			l = fwprintf_num(f, charfmt, w, p, arg.i);
			break;
		}
	}

	if (f) return cnt;
	if (!l10n) return 0;

	for (i=1; i<=NL_ARGMAX && nl_type[i]; i++)
		pop_arg(nl_arg+i, nl_type[i], ap);
	for (; i<=NL_ARGMAX && !nl_type[i]; i++);
	if (i<=NL_ARGMAX) return -1;
	return 1;

inval:
	errno = EINVAL;
	return -1;
overflow:
	errno = EOVERFLOW;
	return -1;
}

c2go_extern
int vfwprintf(FILE *restrict f, const wchar_t *restrict fmt, va_list ap)
{
	va_list ap2;
	int nl_type[NL_ARGMAX+1] = {0};
	union arg nl_arg[NL_ARGMAX+1];
	int olderr;
	int ret;

	/* the copy allows passing va_list* even if va_list is an array */
	va_copy(ap2, ap);
	if (wprintf_core(0, fmt, &ap2, nl_arg, nl_type) < 0) {
		va_end(ap2);
		return -1;
	}

	FLOCK(f);
	__fwide_wide(f);            /* musl fwide(f,1); non-locking under the FLOCK */
	olderr = f->flags & F_ERR;
	f->flags &= ~F_ERR;
	ret = wprintf_core(f, fmt, &ap2, nl_arg, nl_type);
	if (f->flags & F_ERR) ret = -1;   /* musl ferror(f); the non-locking test */
	f->flags |= olderr;
	FUNLOCK(f);
	va_end(ap2);
	return ret;
}

/* ── swprintf / vswprintf (wide string sink, musl src/stdio/vswprintf.c) ──
 * The narrow snprintf's `struct cookie`/`sn_write` are taken, so the wide sink
 * is `struct wcookie`/`sw_write`. The wprintf core emits the format as UTF-8
 * bytes into the stack FILE's byte buffer; sw_write decodes those bytes back to
 * wchar_t (mbtowc) into the caller's buffer on each flush. lock=-1 => no real
 * lock (no deadlock concern). */

struct wcookie {
	wchar_t *ws;
	size_t l;
};

static size_t sw_write(FILE *f, const unsigned char *s, size_t l)
{
	size_t l0 = l;
	int err = 0;
	struct wcookie *c = f->cookie;
	if (s!=f->wbase && sw_write(f, f->wbase, f->wpos-f->wbase)==-1)
		return -1;
	/* Decode the UTF-8 scratch back to wchar_t. __mbrtoc32 reaches the full scalar,
	 * so a supplementary lands as a UTF-16 surrogate PAIR on Windows (mbtowc would
	 * EILSEQ the 4-byte form __fputwc_unlocked wrote); one unit on the UTF-32
	 * targets. A pair needs two output slots — stop at a char boundary if only one
	 * remains (no split); vswprintf's r>=n check then reports the truncation. */
	while (c->l && l) {
		unsigned cp;
		mbstate_t st = {0};
		size_t r = __mbrtoc32(&cp, (void *)s, l, &st);
		if (r==(size_t)-1) { err = 1; break; }
		if (r==(size_t)-2) break;
		if (!r) r = 1;                       /* embedded NUL: consume one byte */
		if (WCHAR_UTF16 && cp>=0x10000u) {
			if (c->l < 2) break;
			c->ws[0] = SURR_HIGH(cp);
			c->ws[1] = SURR_LOW(cp);
			c->ws += 2;
			c->l -= 2;
		} else {
			*c->ws++ = cp;
			c->l--;
		}
		s += r;
		l -= r;
	}
	*c->ws = 0;
	if (err) {
		f->wpos = f->wbase = f->wend = 0;
		f->flags |= F_ERR;
		return -1;
	}
	f->wend = f->buf + f->buf_size;
	f->wpos = f->wbase = f->buf;
	return l0;
}

c2go_extern
int vswprintf(wchar_t *restrict s, size_t n, const wchar_t *restrict fmt, va_list ap)
{
	/* musl vswprintf.c guard (#657). */
	if (n > INT_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	int r;
	unsigned char buf[256];
	struct wcookie c = { s, n-1 };
	FILE f = {
		.lbf = EOF,
		.write = sw_write,
		.lock = -1,
		.buf = buf,
		.buf_size = sizeof buf,
		.cookie = &c,
	};

	if (!n) {
		return -1;
	}
	r = vfwprintf(&f, fmt, ap);
	sw_write(&f, 0, 0);
	return r>=n ? -1 : r;
}

c2go_extern
int swprintf(wchar_t *restrict s, size_t n, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vswprintf(s, n, fmt, ap);
	va_end(ap);
	return ret;
}

/* ── fwprintf / wprintf / vwprintf (thin wrappers, musl) ────────────────── */

c2go_extern
int fwprintf(FILE *restrict f, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfwprintf(f, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int vwprintf(const wchar_t *restrict fmt, va_list ap)
{
	return vfwprintf(stdout, fmt, ap);
}

c2go_extern
int wprintf(const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vwprintf(fmt, ap);
	va_end(ap);
	return ret;
}

/* ── wide formatted input (musl src/stdio/vfwscanf.c + the swscanf/fwscanf/
 *    wscanf/vwscanf/vswscanf wrappers) ──────────────────────────────────────
 *
 * Faithful port of musl's vfwscanf core and its wrappers. store_int, the
 * SIZE_* macros and arg_n are REUSED from the narrow vfscanf above (same TU);
 * in_set is new (transcribed from vfwscanf.c). iswdigit and iswspace both come
 * from <wctype.h>.
 *
 * c2go deviations from musl (the ONLY ones):
 *   1. NUMERIC conversions (d i o u x X a e f g A E F G p): musl builds a temp
 *      format and calls fscanf(f, tmp, ...). fscanf re-takes the FILE lock, but
 *      the c2go FILE lock is NON-reentrant (unlike musl's recursive __lockfile),
 *      so under vfwscanf's FLOCK that self-deadlocks. Instead the numeric cases
 *      are handled INLINE via the scan primitives (shlim/__intscan/__floatscan),
 *      mirroring the narrow vfscanf's own numeric block. `x`/`y`/`base` locals
 *      are added for that (as in narrow vfscanf).
 *   2. The getwc/ungetwc fast macros' slow-path fallback calls the NON-locking
 *      __fgetwc_unlocked / __ungetwc_unlocked (musl falls back to the PUBLIC
 *      LOCKING getwc/ungetwc, which would deadlock under FLOCK).
 *   3. fwide(f,1) -> __fwide_wide(f) (non-locking, defined above).
 *   4. weak_alias(...,__isoc99_*) dropped (c2go has no weak aliases). */

static int in_set(const wchar_t *set, int c)
{
	int j;
	const wchar_t *p = set;
	if (*p == '-') {
		if (c=='-') return 1;
		p++;
	} else if (*p == ']') {
		if (c==']') return 1;
		p++;
	}
	for (; *p && *p != ']'; p++) {
		if (*p=='-' && p[1] && p[1] != ']')
			for (j=p++[-1]; j<*p; j++)
				if (c==j) return 1;
		if (c==*p) return 1;
	}
	return 0;
}

/* Fast getwc/ungetwc — DEVIATION #2: the slow-path fallback calls the NON-locking
 * unlocked twins (musl calls the public locking functions, which self-deadlock
 * under the FLOCK held below). Kept local to the wide-scanf region: they follow
 * every earlier getwc/ungetwc *definition* in this TU, so nothing above is
 * disturbed.
 *
 * The fast path must yield to a PARKED low surrogate (#655 H1): after a
 * supplementary decode on a UTF-16 target, f->wpend holds the low half and
 * rpos already points past the 4 UTF-8 bytes — an ASCII byte there must not
 * jump the queue. The gate folds away on UTF-32 targets (WCHAR_UTF16 == 0). */
#undef getwc
#define getwc(f) \
	(!(WCHAR_UTF16 && (f)->wpend) && (f)->rpos != (f)->rend && *(f)->rpos < 128 \
		? *(f)->rpos++ : __fgetwc_unlocked(f))
#undef ungetwc
#define ungetwc(c,f) \
	((f)->rend && (c)<128U ? *--(f)->rpos : __ungetwc_unlocked((c),(f)))

/* Push the supplementary whose LOW half is parked in f->wpend (by the getwc that
 * returned `high`) back into the read stream, so the next getwc re-yields the
 * whole pair. The wide FILE's single wpend slot cannot hold BOTH halves for
 * ungetwc, so re-encode the pair to its 4 UTF-8 bytes into the already-consumed
 * read buffer and rewind rpos over them (they were just read from there). Needs 4
 * bytes of backup room; returns 0, or -1 if there is none (caller falls back).
 * Only reached on WCHAR_UTF16 (folds away on the UTF-32 targets). */
static int __ungetwc_pair(FILE *f, wint_t high)
{
	if (!f->rpos || f->rpos - f->buf < 4) return -1;
	f->rpos -= 4;
	__surrogate_to_utf8((char *)f->rpos, high, f->wpend);
	f->wpend = 0;
	return 0;
}

/* ungetwc for the wide-scanf peek/exit sites: also restores a supplementary whose
 * low half is parked in wpend (a getwc split it and returned the high). The single
 * wpend slot cannot hold the low AND an ungetwc'd high, so push the whole PAIR
 * back as bytes via __ungetwc_pair. Every peek (getwc-then-ungetwc) below MUST use
 * this — a plain ungetwc(high) with wpend occupied returns WEOF, dropping the high
 * and leaving a lone low surrogate in the stream. Folds to plain ungetwc on the
 * UTF-32 targets and whenever c is not a wpend-pending high surrogate. */
static wint_t unget_wide(wint_t c, FILE *f)
{
	if (WCHAR_UTF16 && IS_HIGH_SURR(c) && f->wpend && __ungetwc_pair(f, c) == 0)
		return c;
	return ungetwc(c, f);
}

c2go_extern
int vfwscanf(FILE *restrict f, const wchar_t *restrict fmt, va_list ap)
{
	int width;
	int size;
	int alloc;
	const wchar_t *p;
	int c, t;
	char *s;
	wchar_t *wcs;
	void *dest=NULL;
	int invert;
	int matches=0;
	off_t pos = 0;
	char tmp[3*sizeof(int)+10];
	const wchar_t *set;
	size_t i, k;
	unsigned long long x;   /* DEVIATION #1: inline numeric scan locals */
	double y;
	int base;

	FLOCK(f);

	__fwide_wide(f);         /* DEVIATION #3: non-locking fwide(f,1) under FLOCK */

	for (p=fmt; *p; p++) {

		alloc = 0;

		if (iswspace(*p)) {
			while (iswspace(p[1])) p++;
			while (iswspace((c=getwc(f)))) pos++;
			unget_wide(c, f);
			continue;
		}
		if (*p != '%' || p[1] == '%') {
			if (*p == '%') {
				p++;
				while (iswspace((c=getwc(f)))) pos++;
			} else {
				c = getwc(f);
			}
			if (c!=*p) {
				unget_wide(c, f);
				if (c<0) goto input_fail;
				goto match_fail;
			}
			pos++;
			continue;
		}

		p++;
		if (*p=='*') {
			dest = 0; p++;
		} else if (iswdigit(*p) && p[1]=='$') {
			dest = arg_n(ap, *p-'0'); p+=2;
		} else {
			dest = va_arg(ap, void *);
		}

		for (width=0; iswdigit(*p); p++) {
			width = 10*width + *p - '0';
		}

		if (*p=='m') {
			wcs = 0;
			s = 0;
			alloc = !!dest;
			p++;
		} else {
			alloc = 0;
		}

		size = SIZE_def;
		switch (*p++) {
		case 'h':
			if (*p == 'h') p++, size = SIZE_hh;
			else size = SIZE_h;
			break;
		case 'l':
			if (*p == 'l') p++, size = SIZE_ll;
			else size = SIZE_l;
			break;
		case 'j':
			size = SIZE_ll;
			break;
		case 'z':
		case 't':
#if defined(_WIN32)
			size = SIZE_ll; /* LLP64: size_t/ptrdiff_t are 8 bytes but long is 4 */
#else
			size = SIZE_l;
#endif
			break;
		case 'L':
			size = SIZE_L;
			break;
		case 'd': case 'i': case 'o': case 'u': case 'x':
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': case 'X':
		case 's': case 'c': case '[':
		case 'S': case 'C':
		case 'p': case 'n':
			p--;
			break;
		default:
			goto fmt_fail;
		}

		t = *p;

		/* Transform S,C -> ls,lc */
		if ((t&0x2f)==3) {
			size = SIZE_l;
			t |= 32;
		}

		if (t != 'n') {
			if (t != '[' && (t|32) != 'c')
				while (iswspace((c=getwc(f)))) pos++;
			else
				c=getwc(f);
			if (c < 0) goto input_fail;
			unget_wide(c, f);
		}

		switch (t) {
		case 'n':
			store_int(dest, size, pos);
			/* do not increment match count, etc! */
			continue;

		case 's':
		case 'c':
		case '[':
			if (t == 'c') {
				if (width<1) width = 1;
				invert = 1;
				set = L"";
			} else if (t == 's') {
				invert = 1;
				static const wchar_t spaces[] = {
					' ', '\t', '\n', '\r', 11, 12,  0x0085,
					0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
					0x2006, 0x2008, 0x2009, 0x200a,
					0x2028, 0x2029, 0x205f, 0x3000, 0 };
				set = spaces;
			} else {
				if (*++p == '^') p++, invert = 1;
				else invert = 0;
				set = p;
				if (*p==']') p++;
				while (*p!=']') {
					if (!*p) goto fmt_fail;
					p++;
				}
			}

			s = (size == SIZE_def) ? dest : 0;
			wcs = (size == SIZE_l) ? dest : 0;

			int gotmatch = 0;

			if (width < 1) width = -1;

			i = 0;
			if (alloc) {
				k = t=='c' ? width+1U : 31;
				if (size == SIZE_l) {
					wcs = malloc(k*sizeof(wchar_t));
					if (!wcs) goto alloc_fail;
				} else {
					s = malloc(k);
					if (!s) goto alloc_fail;
				}
			}
			while (width) {
				if ((c=getwc(f))<0) break;
				if (in_set(set, c) == invert)
					break;
				if (wcs) {
					wcs[i++] = c;
					if (alloc && i==k) {
						k += k+1;
						wchar_t *tmp2 = realloc(wcs, k*sizeof(wchar_t));
						if (!tmp2) goto alloc_fail;
						wcs = tmp2;
					}
				} else if (size != SIZE_l) {
					char *d = s ? s+i : tmp;
					int l;
					if (WCHAR_UTF16 && IS_HIGH_SURR(c)) {
						/* Supplementary: the low half is the next code unit. Require
						 * field room for BOTH units (else break, leaving the pair via
						 * the loop-exit __ungetwc_pair), read the low, and RE-CHECK it
						 * against the scanset -- a supplementary is a member only if
						 * BOTH units are; matching just the high wrongly accepts a
						 * different supplementary sharing the high surrogate (task
						 * #632). Charge width/pos for the low too (loop tail charges
						 * the high). */
						int lo;
						if (width >= 0 && width < 2) break;
						if ((lo=getwc(f))<0 || !IS_LOW_SURR(lo)) {
							if (lo>=0) ungetwc(lo, f);
							goto input_fail;
						}
						if (in_set(set, lo) == invert) {
							ungetwc(lo, f);   /* re-park low; loop-exit restores the pair */
							break;
						}
						l = __surrogate_to_utf8(d, c, lo);
						pos++;
						width-=(width>0);
					} else {
						l = wctomb(d, c);
					}
					if (l<0) goto input_fail;
					i += l;
					if (alloc && i > k-4) {
						k += k+1;
						char *tmp2 = realloc(s, k);
						if (!tmp2) goto alloc_fail;
						s = tmp2;
					}
				}
				pos++;
				width-=(width>0);
				gotmatch=1;
			}
			if (width) {
				/* Push back the char that stopped the match (unget_wide restores a
				 * whole supplementary pair when its low is parked in wpend). */
				unget_wide(c, f);
				if (t == 'c' || !gotmatch) goto match_fail;
			}

			if (alloc) {
				if (size == SIZE_l) *(wchar_t **)dest = wcs;
				else *(char **)dest = s;
			}
			if (t != 'c') {
				if (wcs) wcs[i] = 0;
				if (s) s[i] = 0;
			}
			break;

		case 'd': case 'i': case 'o': case 'u': case 'x':
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': case 'X':
		case 'p':
			/* DEVIATION #1: numeric handled INLINE (musl uses fscanf, which
			 * re-locks the non-reentrant FILE lock -> deadlock). Mirrors the
			 * narrow vfscanf numeric block. Leading whitespace was already
			 * skipped by the t!='n' pre-switch block above. */
			if (width < 1) width = 0;      /* 0 = unlimited (shlim semantics) */
			shlim(f, width);
			if (shgetc(f) < 0) goto input_fail;
			shunget(f);
			switch (t) {
			case 'p': case 'X': case 'x': base = 16; goto int_common_w;
			case 'o':                     base = 8;  goto int_common_w;
			case 'd': case 'u':           base = 10; goto int_common_w;
			case 'i':                     base = 0;
			int_common_w:
				x = __intscan(f, base, 0, ULLONG_MAX);
				if (!shcnt(f)) goto match_fail;
				if (t=='p' && dest) *(void **)dest = (void *)(uintptr_t)x;
				else store_int(dest, size, x);
				break;
			default: /* a e f g A E F G */
				y = __floatscan(f, size, 0);
				if (!shcnt(f)) goto match_fail;
				if (dest) switch (size) {
				case SIZE_def: *(float *)dest = y; break;
				case SIZE_l:   *(double *)dest = y; break;
				case SIZE_L:   *(double *)dest = y; break; /* long double==double */
				}
				break;
			}
			pos += shcnt(f);
			break;
		default:
			goto fmt_fail;
		}

		if (dest) matches++;
	}
	if (0) {
fmt_fail:
alloc_fail:
input_fail:
		if (!matches) matches--;
match_fail:
		if (alloc) {
			free(s);
			free(wcs);
		}
	}
	FUNLOCK(f);
	return matches;
}

/* ── wide scanf family public wrappers (musl fwscanf.c/wscanf.c/…) ───────── */

c2go_extern
int fwscanf(FILE *restrict f, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfwscanf(f, fmt, ap);
	va_end(ap);
	return ret;
}

c2go_extern
int vwscanf(const wchar_t *restrict fmt, va_list ap)
{
	return vfwscanf(stdin, fmt, ap);
}

c2go_extern
int wscanf(const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vwscanf(fmt, ap);
	va_end(ap);
	return ret;
}

/* musl vswscanf.c wstring_read: feed the pseudo-FILE's byte buffer by converting
 * the wide source string (f->cookie) to UTF-8 via wcsrtombs, one refill at a
 * time. lock=-1 => no real lock (no deadlock concern). */
static size_t wstring_read(FILE *f, unsigned char *buf, size_t len)
{
	const wchar_t *src = f->cookie;
	size_t k;

	if (!src) return 0;

	k = wcsrtombs((void *)f->buf, &src, f->buf_size, 0);
	if (k==(size_t)-1) {
		f->rpos = f->rend = 0;
		return 0;
	}

	f->rpos = f->buf;
	f->rend = f->buf + k;
	f->cookie = (void *)src;

	if (!len || !k) return 0;

	*buf = *f->rpos++;
	return 1;
}

c2go_extern
int vswscanf(const wchar_t *restrict s, const wchar_t *restrict fmt, va_list ap)
{
	unsigned char buf[256];
	FILE f;
	memset(&f, 0, sizeof f);
	f.buf = buf;
	f.buf_size = sizeof buf;
	f.cookie = (void *)s;
	f.read = wstring_read;
	f.lock = -1;
	return vfwscanf(&f, fmt, ap);
}

c2go_extern
int swscanf(const wchar_t *restrict s, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vswscanf(s, fmt, ap);
	va_end(ap);
	return ret;
}

/* ── stdio long tail: BSD setvbuf shorthands + legacy word I/O (musl
 * one-liners) + psignal ──────────────────────────────────────────── */

c2go_extern void setbuffer(FILE *f, char *buf, size_t size)
{
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, size);
}

c2go_extern void setlinebuf(FILE *f)
{
	setvbuf(f, 0, _IOLBF, 0);
}

c2go_extern int getw(FILE *f)
{
	int x;
	return fread(&x, sizeof x, 1, f) ? x : EOF;
}

c2go_extern int putw(int x, FILE *f)
{
	return (int)fwrite(&x, sizeof x, 1, f)-1;
}

#if !defined(_WIN32)
/* musl src/signal/psignal.c — lives here (not signal.c) because it works
 * FILE internals under FLOCK. strsignal comes cross-TU through <string.h>'s
 * linkname declaration (the #672 spelling). Unix-only with strsignal.
 * Delta: the f->locale save/restore is kept for shape but is always-null
 * here (single C locale). */
#include <string.h>
c2go_extern void psignal(int sig, const char *msg)
{
	FILE *f = stderr;
	char *s = strsignal(sig);

	FLOCK(f);

	/* Save stderr's orientation and encoding rule, since psignal is not
	 * permitted to change them. Save errno and restore it if there is no
	 * error since fprintf might change it even on success but psignal is
	 * not permitted to do so. */
	void *old_locale = f->locale;
	int old_mode = f->mode;
	int old_errno = errno;

	if (fprintf(f, "%s%s%s\n", msg?msg:"", msg?": ":"", s)>=0)
		errno = old_errno;
	f->mode = old_mode;
	f->locale = old_locale;

	FUNLOCK(f);
}
#endif /* !_WIN32 */

/* ── fmemopen / open_memstream / fopencookie (musl src/stdio, verbatim) ──────
 * Memory-backed streams: no fd, no OS call — they operate purely on buffers, so
 * they are cross-platform (unlike the fd-backed FILEs above). The FILE + its
 * private cookie live in one malloc block (rooted like every FILE, #596); the
 * op fps are these internal statics, wired exactly as __fdopen wires the fd ops.
 * See the constraint note in <stdio.h> for the caller-keepalive / callback-ABI
 * rules. musl's `struct cookie` names are prefixed (mem_/ms_/f) to not clash
 * with the snprintf/vswprintf sink cookies already defined above; the
 * `if (!libc.threaded) f->lock=-1` line is dropped (this port uses the lazy
 * per-FILE lock, #659, like __fdopen). */

struct mem_cookie {
	size_t pos, len, size;
	unsigned char *buf;
	int mode;
};

struct mem_FILE {
	FILE f;
	struct mem_cookie c;
	unsigned char buf[UNGET+BUFSIZ], buf2[];
};

static off_t mem_seek(FILE *f, off_t off, int whence)
{
	ssize_t base;
	struct mem_cookie *c = f->cookie;
	if (whence>2U) {
fail:
		errno = EINVAL;
		return -1;
	}
	base = (size_t [3]){0, c->pos, c->len}[whence];
	if (off < -base || off > (ssize_t)c->size-base) goto fail;
	return c->pos = base+off;
}

static size_t mem_read(FILE *f, unsigned char *buf, size_t len)
{
	struct mem_cookie *c = f->cookie;
	size_t rem = c->len - c->pos;
	if (c->pos > c->len) rem = 0;
	if (len > rem) {
		len = rem;
		f->flags |= F_EOF;
	}
	memcpy(buf, c->buf+c->pos, len);
	c->pos += len;
	rem -= len;
	if (rem > f->buf_size) rem = f->buf_size;
	f->rpos = f->buf;
	f->rend = f->buf + rem;
	memcpy(f->rpos, c->buf+c->pos, rem);
	c->pos += rem;
	return len;
}

static size_t mem_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct mem_cookie *c = f->cookie;
	size_t rem;
	size_t len2 = f->wpos - f->wbase;
	if (len2) {
		f->wpos = f->wbase;
		if (mem_write(f, f->wpos, len2) < len2) return 0;
	}
	if (c->mode == 'a') c->pos = c->len;
	rem = c->size - c->pos;
	if (len > rem) len = rem;
	memcpy(c->buf+c->pos, buf, len);
	c->pos += len;
	if (c->pos > c->len) {
		c->len = c->pos;
		if (c->len < c->size) c->buf[c->len] = 0;
		else if ((f->flags&F_NORD) && c->size) c->buf[c->size-1] = 0;
	}
	return len;
}

static int mem_close(FILE *m)
{
	return 0;
}

c2go_extern
FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode)
{
	struct mem_FILE *f;
	int plus = !!strchr(mode, '+');

	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	if (!buf && size > PTRDIFF_MAX) {
		errno = ENOMEM;
		return 0;
	}

	f = malloc(sizeof *f + (buf?0:size));
	if (!f) return 0;
	memset(f, 0, offsetof(struct mem_FILE, buf));
	f->f.cookie = &f->c;
	f->f.fd = -1;
	f->f.lbf = EOF;
	f->f.buf = f->buf + UNGET;
	f->f.buf_size = sizeof f->buf - UNGET;
	if (!buf) {
		buf = f->buf2;
		memset(buf, 0, size);
	}

	f->c.buf = buf;
	f->c.size = size;
	f->c.mode = *mode;

	if (!plus) f->f.flags = (*mode == 'r') ? F_NOWR : F_NORD;
	if (*mode == 'r') f->c.len = size;
	else if (*mode == 'a') f->c.len = f->c.pos = strnlen(buf, size);
	else if (plus) *f->c.buf = 0;

	f->f.read = mem_read;
	f->f.write = mem_write;
	f->f.seek = mem_seek;
	f->f.close = mem_close;

	return __ofl_add(&f->f);
}

struct ms_cookie {
	char **bufp;
	size_t *sizep;
	size_t pos;
	char *buf;
	size_t len;
	size_t space;
};

struct ms_FILE {
	FILE f;
	struct ms_cookie c;
	unsigned char buf[BUFSIZ];
};

static off_t ms_seek(FILE *f, off_t off, int whence)
{
	ssize_t base;
	struct ms_cookie *c = f->cookie;
	if (whence>2U) {
fail:
		errno = EINVAL;
		return -1;
	}
	base = (size_t [3]){0, c->pos, c->len}[whence];
	if (off < -base || off > SSIZE_MAX-base) goto fail;
	return c->pos = base+off;
}

static size_t ms_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct ms_cookie *c = f->cookie;
	size_t len2 = f->wpos - f->wbase;
	char *newbuf;
	if (len2) {
		f->wpos = f->wbase;
		if (ms_write(f, f->wbase, len2) < len2) return 0;
	}
	if (len + c->pos >= c->space) {
		len2 = 2*c->space+1 | c->pos+len+1;
		newbuf = realloc(c->buf, len2);
		if (!newbuf) return 0;
		*c->bufp = c->buf = newbuf;
		memset(c->buf + c->space, 0, len2 - c->space);
		c->space = len2;
	}
	memcpy(c->buf+c->pos, buf, len);
	c->pos += len;
	if (c->pos >= c->len) c->len = c->pos;
	*c->sizep = c->pos;
	return len;
}

static int ms_close(FILE *f)
{
	return 0;
}

c2go_extern
FILE *open_memstream(char **bufp, size_t *sizep)
{
	struct ms_FILE *f;
	char *buf;

	if (!(f=malloc(sizeof *f))) return 0;
	if (!(buf=malloc(sizeof *buf))) {
		free(f);
		return 0;
	}
	memset(&f->f, 0, sizeof f->f);
	memset(&f->c, 0, sizeof f->c);
	f->f.cookie = &f->c;

	f->c.bufp = bufp;
	f->c.sizep = sizep;
	f->c.pos = f->c.len = f->c.space = *sizep = 0;
	f->c.buf = *bufp = buf;
	*buf = 0;

	f->f.flags = F_NORD;
	f->f.fd = -1;
	f->f.buf = f->buf;
	f->f.buf_size = sizeof f->buf;
	f->f.lbf = EOF;
	f->f.write = ms_write;
	f->f.seek = ms_seek;
	f->f.close = ms_close;
	f->f.mode = -1;

	return __ofl_add(&f->f);
}

/* fopencookie — the caller's io callbacks (fc->iofuncs.*) are invoked through
 * c2go's internal ABI, so they must be c2go-compiled C functions (see the
 * <stdio.h> note). musl's #include <sys/ioctl.h>/<fcntl.h> are dropped: the
 * code never touches ioctl/fcntl. */

struct fcookie {
	void *cookie;
	cookie_io_functions_t iofuncs;
};

struct cookie_FILE {
	FILE f;
	struct fcookie fc;
	unsigned char buf[UNGET+BUFSIZ];
};

static size_t cookieread(FILE *f, unsigned char *buf, size_t len)
{
	struct fcookie *fc = f->cookie;
	ssize_t ret = -1;
	size_t remain = len, readlen = 0;
	size_t len2 = len - !!f->buf_size;

	if (!fc->iofuncs.read) goto bail;

	if (len2) {
		ret = fc->iofuncs.read(fc->cookie, (char *) buf, len2);
		if (ret <= 0) goto bail;

		readlen += ret;
		remain -= ret;
	}

	if (!f->buf_size || remain > !!f->buf_size) return readlen;

	f->rpos = f->buf;
	ret = fc->iofuncs.read(fc->cookie, (char *) f->rpos, f->buf_size);
	if (ret <= 0) goto bail;
	f->rend = f->rpos + ret;

	buf[readlen++] = *f->rpos++;

	return readlen;

bail:
	f->flags |= ret == 0 ? F_EOF : F_ERR;
	f->rpos = f->rend = f->buf;
	return readlen;
}

static size_t cookiewrite(FILE *f, const unsigned char *buf, size_t len)
{
	struct fcookie *fc = f->cookie;
	ssize_t ret;
	size_t len2 = f->wpos - f->wbase;
	if (!fc->iofuncs.write) return len;
	if (len2) {
		f->wpos = f->wbase;
		if (cookiewrite(f, f->wpos, len2) < len2) return 0;
	}
	ret = fc->iofuncs.write(fc->cookie, (const char *) buf, len);
	if (ret < 0) {
		f->wpos = f->wbase = f->wend = 0;
		f->flags |= F_ERR;
		return 0;
	}
	return ret;
}

static off_t cookieseek(FILE *f, off_t off, int whence)
{
	struct fcookie *fc = f->cookie;
	int res;
	if (whence > 2U) {
		errno = EINVAL;
		return -1;
	}
	if (!fc->iofuncs.seek) {
		errno = ENOTSUP;
		return -1;
	}
	res = fc->iofuncs.seek(fc->cookie, &off, whence);
	if (res < 0)
		return res;
	return off;
}

static int cookieclose(FILE *f)
{
	struct fcookie *fc = f->cookie;
	if (fc->iofuncs.close) return fc->iofuncs.close(fc->cookie);
	return 0;
}

c2go_extern
FILE *fopencookie(void *cookie, const char *mode, cookie_io_functions_t iofuncs)
{
	struct cookie_FILE *f;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+fcookie+buffer or fail */
	if (!(f=malloc(sizeof *f))) return 0;

	/* Zero-fill only the struct, not the buffer */
	memset(&f->f, 0, sizeof f->f);

	/* Impose mode restrictions */
	if (!strchr(mode, '+')) f->f.flags = (*mode == 'r') ? F_NOWR : F_NORD;

	/* Set up our fcookie */
	f->fc.cookie = cookie;
	f->fc.iofuncs = iofuncs;

	f->f.fd = -1;
	f->f.cookie = &f->fc;
	f->f.buf = f->buf + UNGET;
	f->f.buf_size = sizeof f->buf - UNGET;
	f->f.lbf = EOF;

	/* Initialize op ptrs. No problem if some are unneeded. */
	f->f.read = cookieread;
	f->f.write = cookiewrite;
	f->f.seek = cookieseek;
	f->f.close = cookieclose;

	/* Add new FILE to open file list */
	return __ofl_add(&f->f);
}
