/* fsops_posix.c — the remaining POSIX fd / path operations (Unix): positional
 * I/O, descriptor duplication, pipes, sync, truncation, tty test, and the
 * symlink / cwd family. Same shape as source/io_posix.c and source/stat_posix.c:
 * musl-style C wrappers over one Go bridge each (__c2go_syscall_*, fsops.go /
 * fsops_darwin.go / fsops_linux.go), applying musl's __syscall_ret conversion
 * `if (r < 0) { errno = -r; return -1; }`.
 *
 * Buffer-filling ops (pread/readlink/getcwd/pipe) hand the caller's pointer to
 * Go, which writes it only AFTER the blocking syscall returns; the pointer is a
 * tracked Go parameter (copystack-adjusted), nothing is laundered through uintptr.
 *
 * Empty on Windows: the Windows port LIVES in fsops_windows.c (#647), split by
 * the Go-first rule exactly as sketched here — PATH family via Go, fd-table
 * ops via msvcrt/Win32.
 */
#if !defined(_WIN32)

#include <c2go.h>
#include <unistd.h>
#include <fcntl.h>   /* struct flock + F_SETLK for lockf (#664) */   /* the decls + ssize_t/off_t/size_t */
#include <errno.h>    /* errno + ENOTTY (per-OS native values) */
#include <stdlib.h>   /* malloc for realpath(path, NULL) */
#include <limits.h>   /* PATH_MAX */
#include <string.h>   /* strdup for getcwd(NULL) (#655 H3) */

/* Go bridges (fsops.go / fsops_darwin.go / fsops_linux.go). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_pread", C2GO_GOABI0)
long __c2go_syscall_pread(int fd, void *buf, unsigned long n, long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_pwrite", C2GO_GOABI0)
long __c2go_syscall_pwrite(int fd, const void *buf, unsigned long n, long off);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_dup", C2GO_GOABI0)
int  __c2go_syscall_dup(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_pipe", C2GO_GOABI0)
int  __c2go_syscall_pipe(int *fds);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fsync", C2GO_GOABI0)
int  __c2go_syscall_fsync(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fdatasync", C2GO_GOABI0)
int  __c2go_syscall_fdatasync(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_ftruncate", C2GO_GOABI0)
int  __c2go_syscall_ftruncate(int fd, long len);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_truncate", C2GO_GOABI0)
int  __c2go_syscall_truncate(const char *path, long len);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_isatty", C2GO_GOABI0)
int  __c2go_syscall_isatty(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_link", C2GO_GOABI0)
int  __c2go_syscall_link(const char *from, const char *to);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_symlink", C2GO_GOABI0)
int  __c2go_syscall_symlink(const char *target, const char *linkpath);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_readlink", C2GO_GOABI0)
long __c2go_syscall_readlink(const char *path, char *buf, unsigned long bufsiz);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_chdir", C2GO_GOABI0)
int  __c2go_syscall_chdir(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_getcwd", C2GO_GOABI0)
int  __c2go_syscall_getcwd(char *buf, unsigned long size);

c2go_extern ssize_t pread(int fd, void *buf, size_t n, off_t off) {
	long r = __c2go_syscall_pread(fd, buf, n, (long)off);
	if (r < 0) { errno = (int)-r; return -1; }
	return r;
}

c2go_extern ssize_t pwrite(int fd, const void *buf, size_t n, off_t off) {
	long r = __c2go_syscall_pwrite(fd, buf, n, (long)off);
	if (r < 0) { errno = (int)-r; return -1; }
	return r;
}

c2go_extern int dup(int fd) {
	int r = __c2go_syscall_dup(fd);
	if (r < 0) { errno = -r; return -1; }
	return r;
}

c2go_extern int pipe(int fds[2]) {
	int r = __c2go_syscall_pipe(fds);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int fsync(int fd) {
	int r = __c2go_syscall_fsync(fd);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int fdatasync(int fd) {
	int r = __c2go_syscall_fdatasync(fd);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int ftruncate(int fd, off_t len) {
	int r = __c2go_syscall_ftruncate(fd, (long)len);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int truncate(const char *path, off_t len) {
	int r = __c2go_syscall_truncate(path, (long)len);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

/* isatty: 1 for a terminal, 0 otherwise with errno=ENOTTY — except a bad fd
 * keeps EBADF (musl src/unistd/isatty.c distinguishes them; #657). The bridge
 * returns 1 / 0 / -EBADF. */
c2go_extern int isatty(int fd) {
	int r = __c2go_syscall_isatty(fd);
	if (r < 0) { errno = -r; return 0; }
	if (!r) errno = ENOTTY;
	return r;
}

/* flock (#648, <sys/file.h>): BSD whole-file lock on the OPEN FILE
 * DESCRIPTION (syscall.Flock bridge). The LOCK_* bits are identical on
 * linux/macOS, so they pass through natively. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_flock", C2GO_GOABI0)
int __c2go_syscall_flock(int fd, int op);

c2go_extern int flock(int fd, int op) {
	int r = __c2go_syscall_flock(fd, op);
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

/* readlink does NOT NUL-terminate; it returns the byte count (musl/POSIX). */
c2go_extern ssize_t readlink(const char *__restrict path, char *__restrict buf, size_t bufsiz) {
	long r = __c2go_syscall_readlink(path, buf, bufsiz);
	if (r < 0) { errno = (int)-r; return -1; }
	return r;
}

c2go_extern int chdir(const char *path) {
	int r = __c2go_syscall_chdir(path);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

/* getcwd fills buf (NUL-terminated) and returns it, or NULL with errno set
 * (ERANGE when the buffer is too small). */
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

/* realpath: canonicalise via the Go bridge (path/filepath EvalSymlinks + Abs),
 * which resolves symlinks and requires the path to exist. A NULL resolved_path
 * mallocs a PATH_MAX buffer (the POSIX.1-2008 / GNU extension); on error that
 * buffer is freed. The bridge writes at most PATH_MAX bytes (NUL-terminated). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_realpath", C2GO_GOABI0)
int __c2go_realpath(const char *path, char *buf, unsigned long size);

c2go_extern char *realpath(const char *__restrict path, char *__restrict resolved_path) {
	/* musl src/misc/realpath.c guards: NULL path is EINVAL, an empty path is
	 * ENOENT (without these the Go bridge's filepath.Abs("") resolves to the cwd
	 * and returns it as if it were a real path). */
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

/* lockf — musl src/misc/lockf.c verbatim (#664) over the #648 fcntl record
 * locks (uniform c2go struct flock / command codes). */
c2go_extern int lockf(int fd, int op, off_t size)
{
	struct flock l = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_CUR,
		.l_len = size,
	};
	switch (op) {
	case F_TEST:
		l.l_type = F_RDLCK;
		if (fcntl(fd, F_GETLK, &l) < 0)
			return -1;
		if (l.l_type == F_UNLCK || l.l_pid == getpid())
			return 0;
		errno = EACCES;
		return -1;
	case F_ULOCK:
		l.l_type = F_UNLCK;
	case F_TLOCK:
		return fcntl(fd, F_SETLK, &l);
	case F_LOCK:
		return fcntl(fd, F_SETLKW, &l);
	}
	errno = EINVAL;
	return -1;
}

#endif /* !_WIN32 */
