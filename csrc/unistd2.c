/* unistd2.c — #675 stage C: the unix fd/path surface over the Go syscall
 * shims (readv/writev, the *at family, fchdir/fchown/chown/lchown, pipe2/
 * pipe, sync, gethostname) plus the conf wrappers (getpagesize/sysconf/
 * getentropy). Every wrapper is the musl one-liner shape: call the shim,
 * apply __syscall_ret (`if (r < 0) { errno = -r; return -1; }`). Unix-only
 * (mirrors io_posix.c; MinGW has none of these). */
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_readv", C2GO_GOABI0)
long __c2go_syscall_readv(int fd, const struct iovec *iov, int iovcnt);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_writev", C2GO_GOABI0)
long __c2go_syscall_writev(int fd, const struct iovec *iov, int iovcnt);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_openat", C2GO_GOABI0)
long __c2go_syscall_openat(int dirfd, const char *path, int flags, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_mkdirat", C2GO_GOABI0)
long __c2go_syscall_mkdirat(int dirfd, const char *path, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fstatat", C2GO_GOABI0)
long __c2go_syscall_fstatat(int dirfd, const char *path, void *buf, int flags);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_unlinkat", C2GO_GOABI0)
long __c2go_syscall_unlinkat(int dirfd, const char *path, int flags);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_renameat", C2GO_GOABI0)
long __c2go_syscall_renameat(int fromfd, const char *from, int tofd, const char *to);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fchdir", C2GO_GOABI0)
long __c2go_syscall_fchdir(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fchown", C2GO_GOABI0)
long __c2go_syscall_fchown(int fd, unsigned uid, unsigned gid);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_chown", C2GO_GOABI0)
long __c2go_syscall_chown(const char *path, unsigned uid, unsigned gid, int link);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_pipe2", C2GO_GOABI0)
long __c2go_syscall_pipe2(int fds[2], int flags);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_sync", C2GO_GOABI0)
long __c2go_syscall_sync(void);
/* gethostname/getentropy/getpagesize/sysconf live in source/conf.c —
 * cross-platform per the #675 "1+3" decision. */

#define SYSCALL_RET(r) do { if ((r) < 0) { errno = (int)-(r); return -1; } } while (0)

c2go_extern ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
	long r = __c2go_syscall_readv(fd, iov, iovcnt);
	SYSCALL_RET(r);
	return r;
}

c2go_extern ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
	long r = __c2go_syscall_writev(fd, iov, iovcnt);
	SYSCALL_RET(r);
	return r;
}

c2go_extern int openat(int dirfd, const char *path, int flags, ...) {
	unsigned mode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, unsigned);
		va_end(ap);
	}
	long r = __c2go_syscall_openat(dirfd, path, flags, mode);
	SYSCALL_RET(r);
	return (int)r;
}

c2go_extern int mkdirat(int dirfd, const char *path, mode_t mode) {
	long r = __c2go_syscall_mkdirat(dirfd, path, mode);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int fstatat(int dirfd, const char *restrict path, struct stat *restrict st, int flags) {
	long r = __c2go_syscall_fstatat(dirfd, path, st, flags);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int unlinkat(int dirfd, const char *path, int flags) {
	long r = __c2go_syscall_unlinkat(dirfd, path, flags);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int renameat(int fromfd, const char *from, int tofd, const char *to) {
	long r = __c2go_syscall_renameat(fromfd, from, tofd, to);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int fchdir(int fd) {
	long r = __c2go_syscall_fchdir(fd);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int fchown(int fd, uid_t uid, gid_t gid) {
	long r = __c2go_syscall_fchown(fd, uid, gid);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int chown(const char *path, uid_t uid, gid_t gid) {
	long r = __c2go_syscall_chown(path, uid, gid, 0);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int lchown(const char *path, uid_t uid, gid_t gid) {
	long r = __c2go_syscall_chown(path, uid, gid, 1);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int pipe2(int fds[2], int flags) {
	long r = __c2go_syscall_pipe2(fds, flags);
	SYSCALL_RET(r);
	return 0;
}
/* pipe() predates this batch (fsops_posix.c). */

c2go_extern void sync(void) {
	(void)__c2go_syscall_sync();
}

#endif /* !_WIN32 */
