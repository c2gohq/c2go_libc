/* fsops_windows.c — the remaining fd / path operations on Windows (#647), the
 * per-OS counterpart of fsops_posix.c. Split per that file's plan:
 *
 *   - PATH family (truncate/link/symlink/readlink/chdir/getcwd/realpath):
 *     Go-first over ../io_windows.go — os.Truncate / os.Link / os.Symlink /
 *     os.Readlink / os.Chdir / os.Getwd / filepath — exactly like the
 *     unlink/rmdir/rename trio already there. Same shim contract: result or
 *     -errno (MinGW value via winErrno), wrapper applies musl's __syscall_ret.
 *
 *   - fd family: the CRT owns the small-int fd table, so pipe/dup/fsync/
 *     fdatasync/ftruncate go through msvcrt (_pipe/_dup/_commit/_chsize);
 *     pread/pwrite have NO CRT primitive (MinGW-w64 provides none), so the
 *     wrapper recovers the WIN32 HANDLE via _get_osfhandle and Go performs
 *     ReadFile/WriteFile with an OVERLAPPED offset — Win32's native
 *     positional I/O. CAVEAT (documented deviation): on a synchronous handle
 *     the file pointer moves after an OVERLAPPED transfer, unlike POSIX
 *     pread; there is no better primitive short of reopening the file.
 *
 *   - Virtualized std descriptors (0/1/2 — see source/stdio.c): pread/pwrite/
 *     fsync/fdatasync/ftruncate route to the live os.Std* via the portable
 *     helpers; isatty is identity-gated (GetConsoleMode on the startup
 *     object); dup(std) snapshots the live HANDLE via DuplicateHandle and
 *     wraps it into a CRT fd with _open_osfhandle.
 *
 *   - fchmod is NOT provided: MinGW-w64 has none (no CRT fd → chmod path),
 *     and inventing one would be a fake stub. <sys/stat.h> guards it.
 *
 * The whole file is empty off Windows. */
#if defined(_WIN32)

#include <c2go.h>
#include <unistd.h>   /* the decls + ssize_t/off_t/size_t */
#include <fcntl.h>    /* O_BINARY for _pipe */
#include <errno.h>    /* errno (MinGW-native values) */
#include <stdlib.h>   /* malloc/free for realpath(path, NULL) */
#include <limits.h>   /* PATH_MAX */
#include <string.h>   /* strdup for getcwd(NULL) (#655 H3) */

/* msvcrt CRT fd primitives (resolved via the c2go-bind -l msvcrt dynimp). */
extern int       _pipe(int fds[2], unsigned size, int mode);
extern int       _dup(int fd);
extern int       _commit(int fd);
extern long long _get_osfhandle(int fd);
extern int       _open_osfhandle(long long h, int flags);
extern int       _isatty(int fd);
extern int      *_errno(void);

static void map_errno_fs(void) { errno = *_errno(); }

/* Go bridges (../io_windows.go / ../stdio_std.go). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_truncate", C2GO_GOABI0)
int  __c2go_syscall_truncate(const char *path, long long len);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_link", C2GO_GOABI0)
int  __c2go_syscall_link(const char *from, const char *to);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_symlink", C2GO_GOABI0)
int  __c2go_syscall_symlink(const char *target, const char *linkpath);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_readlink", C2GO_GOABI0)
long long __c2go_syscall_readlink(const char *path, char *buf, unsigned long long bufsiz);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_chdir", C2GO_GOABI0)
int  __c2go_syscall_chdir(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_getcwd", C2GO_GOABI0)
int  __c2go_syscall_getcwd(char *buf, unsigned long long size);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_realpath", C2GO_GOABI0)
int  __c2go_realpath(const char *path, char *buf, unsigned long long size);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_closehandle", C2GO_GOABI0)
void __c2go_closehandle(long long h);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pread_h", C2GO_GOABI0)
long long __c2go_pread_h(long long h, void *buf, unsigned long long n, long long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pwrite_h", C2GO_GOABI0)
long long __c2go_pwrite_h(long long h, const void *buf, unsigned long long n, long long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_ftruncate_h", C2GO_GOABI0)
int  __c2go_ftruncate_h(long long h, long long len);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_dup_handle", C2GO_GOABI0)
long long __c2go_std_dup_handle(int which);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_isatty_win", C2GO_GOABI0)
int  __c2go_std_isatty_win(int which);
/* Portable std helpers already linknamed elsewhere reuse their Go names: */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_pread", C2GO_GOABI0)
long long __c2go_std_pread(int which, void *buf, unsigned long long n, long long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_pwrite", C2GO_GOABI0)
long long __c2go_std_pwrite(int which, const void *buf, unsigned long long n, long long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_sync", C2GO_GOABI0)
int  __c2go_std_sync(int which);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_truncate", C2GO_GOABI0)
int  __c2go_std_truncate(int which, long long len);

/* ── PATH family (Go-first) ─────────────────────────────────────────────── */

c2go_extern int truncate(const char *path, off_t len) {
	int r = __c2go_syscall_truncate(path, (long long)len);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int link(const char *from, const char *to) {
	int r = __c2go_syscall_link(from, to);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int symlink(const char *target, const char *linkpath) {
	int r = __c2go_syscall_symlink(target, linkpath);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern ssize_t readlink(const char *__restrict path, char *__restrict buf, size_t bufsiz) {
	long long r = __c2go_syscall_readlink(path, buf, bufsiz);
	if (r < 0) { errno = (int)-r; return -1; }
	return (ssize_t)r;
}

c2go_extern int chdir(const char *path) {
	int r = __c2go_syscall_chdir(path);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern char *getcwd(char *buf, size_t size) {
	/* musl src/unistd/getcwd.c shape (#655 H3): a NULL buf means "allocate"
	 * (the widely-relied-on musl/glibc extension) via a local staging buffer
	 * + strdup; a caller buffer with size 0 is EINVAL. musl uses a VLA sized
	 * buf?1:PATH_MAX; a fixed PATH_MAX array keeps the frame shape static. */
	char tmp[PATH_MAX];
	if (!buf) {
		buf = tmp;
		size = sizeof tmp;
	} else if (!size) {
		errno = EINVAL;
		return (void *)0;
	}
	int r = __c2go_syscall_getcwd(buf, size);
	if (r < 0) { errno = -r; return (void *)0; }
	return buf == tmp ? strdup(buf) : buf;
}

c2go_extern char *realpath(const char *__restrict path, char *__restrict resolved_path) {
	if (!path) { errno = EINVAL; return (void *)0; }
	if (!path[0]) { errno = ENOENT; return (void *)0; }
	char *buf = resolved_path ? resolved_path : malloc(PATH_MAX);
	if (!buf) { errno = ENOMEM; return (void *)0; }
	int r = __c2go_realpath(path, buf, PATH_MAX);
	if (r < 0) {
		if (!resolved_path) free(buf);
		errno = -r;
		return (void *)0;
	}
	return buf;
}

/* ── fd family ──────────────────────────────────────────────────────────── */

c2go_extern int pipe(int fds[2]) {
	if (_pipe(fds, 8192, O_BINARY) != 0) { map_errno_fs(); return -1; }
	return 0;
}

c2go_extern int dup(int fd) {
	if (fd >= 0 && fd <= 2) {
		/* Virtualized std: snapshot the live os.Std* HANDLE (DuplicateHandle)
		 * and wrap it into a fresh CRT fd — POSIX dup's "same open file"
		 * semantics against the CURRENT target. */
		long long h = __c2go_std_dup_handle(fd);
		if (h < 0) { errno = (int)-h; return -1; }
		int nfd = _open_osfhandle(h, O_BINARY); /* #658 M8: match open()'s repo-wide binary mode */
		if (nfd < 0) {
			__c2go_closehandle(h); /* #658 M8: the duplicate would otherwise leak */
			map_errno_fs();
		}
		return nfd;
	}
	int r = _dup(fd);
	if (r < 0) map_errno_fs();
	return r;
}

c2go_extern int fsync(int fd) {
	if (fd >= 0 && fd <= 2) {
		int r = __c2go_std_sync(fd);
		if (r < 0) { errno = -r; return -1; }
		return 0;
	}
	if (_commit(fd) != 0) { map_errno_fs(); return -1; }
	return 0;
}

/* Windows has no fsync/fdatasync distinction; both are _commit. */
c2go_extern int fdatasync(int fd) {
	return fsync(fd);
}

c2go_extern int ftruncate(int fd, off_t len) {
	if (fd >= 0 && fd <= 2) {
		int r = __c2go_std_truncate(fd, (long long)len);
		if (r < 0) { errno = -r; return -1; }
		return 0;
	}
	/* msvcrt's _chsize takes a 32-bit long (no _chsize_s in msvcrt.dll) —
	 * recover the HANDLE and truncate 64-bit clean via Go (SetEndOfFile). */
	long long h = _get_osfhandle(fd);
	if (h == -1) { map_errno_fs(); return -1; }
	int r = __c2go_ftruncate_h(h, (long long)len);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern ssize_t pread(int fd, void *buf, size_t n, off_t off) {
	long long r;
	if (fd >= 0 && fd <= 2) {
		r = __c2go_std_pread(fd, buf, n, (long long)off);
	} else {
		long long h = _get_osfhandle(fd);
		if (h == -1) { map_errno_fs(); return -1; }
		r = __c2go_pread_h(h, buf, n, (long long)off);
	}
	if (r < 0) { errno = (int)-r; return -1; }
	return (ssize_t)r;
}

c2go_extern ssize_t pwrite(int fd, const void *buf, size_t n, off_t off) {
	long long r;
	if (fd >= 0 && fd <= 2) {
		r = __c2go_std_pwrite(fd, buf, n, (long long)off);
	} else {
		long long h = _get_osfhandle(fd);
		if (h == -1) { map_errno_fs(); return -1; }
		r = __c2go_pwrite_h(h, buf, n, (long long)off);
	}
	if (r < 0) { errno = (int)-r; return -1; }
	return (ssize_t)r;
}

/* isatty: identity-gated console probe for the virtualized std descriptors;
 * CRT _isatty (character-device test) for CRT fds. musl-faithful errno. */
c2go_extern int isatty(int fd) {
	int r;
	if (fd >= 0 && fd <= 2)
		r = __c2go_std_isatty_win(fd);
	else
		r = _isatty(fd) ? 1 : 0;
	if (!r) errno = ENOTTY;
	return r;
}

#endif /* _WIN32 */
