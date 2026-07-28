/* io_posix.c — the POSIX fd-I/O layer (Unix).
 *
 * musl-shaped C wrappers: a POSIX return value plus errno on failure. Each
 * composes exactly one Go syscall shim (__c2go_syscall_*, io.go / io_darwin.go /
 * io_linux.go) — issuing a raw trap from C would bypass Go's syscall scheduler
 * protocol, so the trap comes from Go's syscall package (which also does the
 * park/unpark around the blocking call). The shim returns the syscall result on
 * success or -errno on failure (raw Linux convention); these wrappers apply
 * musl's __syscall_ret conversion: `if (r < 0) { errno = -r; return -1; }`. This
 * is the symmetric counterpart of source/io_windows.c, which instead calls the
 * msvcrt CRT.
 *
 * open/fcntl are variadic: c2go lowers the call through the void** tagged
 * argument pack, so va_arg reads the pack cell (exactly as the printf/scanf
 * family does); the extracted mode/arg is handed to the non-variadic shim. open
 * reads its mode only under O_CREAT (musl-faithful — a no-mode open passes none).
 * fcntl reads its arg unconditionally like musl: a no-arg command (F_GETFD) lands
 * on the pack's nil sentinel, which the c2go va_arg lowering resolves to 0 (#613
 * null-tolerant) instead of faulting.
 *
 * The whole file is empty on Windows (gen.sh globs every source for every
 * target); io_windows.c carries the Windows implementation.
 */
#if !defined(_WIN32)

#include <c2go.h>
#include <unistd.h>   /* read/write/close/lseek/dup2/dup3 decls + ssize_t/off_t/size_t */
#include <fcntl.h>    /* open/fcntl decls + O_* flag values (native per-OS) */
#include <errno.h>    /* errno (per-OS native values) */
#include <stdarg.h>   /* va_list for the variadic open/fcntl */

/* Go syscall shims (io.go / io_darwin.go / io_linux.go). Each returns the
 * syscall result on success or -errno on failure. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_read", C2GO_GOABI0)
long __c2go_syscall_read(int fd, void *buf, unsigned long n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_write", C2GO_GOABI0)
long __c2go_syscall_write(int fd, const void *buf, unsigned long n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_open", C2GO_GOABI0)
int  __c2go_syscall_open(const char *path, int flags, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_close", C2GO_GOABI0)
int  __c2go_syscall_close(int fd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_lseek", C2GO_GOABI0)
long __c2go_syscall_lseek(int fd, long off, int whence);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fcntl", C2GO_GOABI0)
int  __c2go_syscall_fcntl(int fd, int cmd, int arg);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_dup2", C2GO_GOABI0)
int  __c2go_syscall_dup2(int oldfd, int newfd);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_dup3", C2GO_GOABI0)
int  __c2go_syscall_dup3(int oldfd, int newfd, int flags);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_unlink", C2GO_GOABI0)
int  __c2go_syscall_unlink(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_rmdir", C2GO_GOABI0)
int  __c2go_syscall_rmdir(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_rename", C2GO_GOABI0)
int  __c2go_syscall_rename(const char *from, const char *to);

c2go_extern ssize_t read(int fd, void *buf, size_t n) {
	long r = __c2go_syscall_read(fd, buf, n);
	if (r < 0) { errno = (int)-r; return -1; }
	return r;
}

c2go_extern ssize_t write(int fd, const void *buf, size_t n) {
	long r = __c2go_syscall_write(fd, buf, n);
	if (r < 0) { errno = (int)-r; return -1; }
	return r;
}

/* open(path, flags, ...): the vararg mode is present only with O_CREAT. */
c2go_extern int open(const char *path, int flags, ...) {
	unsigned mode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, unsigned);
		va_end(ap);
	}
	int r = __c2go_syscall_open(path, flags, mode);
	if (r < 0) { errno = -r; return -1; }
	return r;
}

c2go_extern int close(int fd) {
	/* fds 0/1/2 are owned by Go's os package (the std streams route through the
	 * live os.Stdin/Stdout/Stderr — see stdio.c): releasing the NUMBER would let
	 * the next open() land on it, after which Go-side writers silently corrupt
	 * that file. Refuse (documented POSIX deviation); redirect with dup2/freopen
	 * instead — both keep the number occupied. */
	if (fd >= 0 && fd <= 2) { errno = EBADF; return -1; }
	int r = __c2go_syscall_close(fd);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern off_t lseek(int fd, off_t off, int whence) {
	long r = __c2go_syscall_lseek(fd, (long)off, whence);
	if (r < 0) { errno = (int)-r; return -1; }
	return (off_t)r;
}

/* Record-lock bridge (#648, ../flock.go): translates the c2go-uniform command
 * codes + struct flock layout to the host's syscall.FcntlFlock. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_fcntl_flock", C2GO_GOABI0)
int  __c2go_fcntl_flock(int fd, int cmd, struct flock *lk);

/* fcntl(fd, cmd, ...): the RECORD-LOCK commands (F_GETLK/F_SETLK/F_SETLKW,
 * c2go-uniform codes — see <fcntl.h>) take a struct flock* and route to the
 * dedicated Go bridge; every other command reads the optional int arg
 * UNCONDITIONALLY, exactly as musl does (src/fcntl/fcntl.c) — a no-arg
 * command (F_GETFD/F_GETFL) makes va_arg land on the pack's nil sentinel,
 * which the c2go va_arg lowering resolves to 0 (#613 null-tolerant). Command
 * codes and F_SETFL flag bits for the pass-through set are host-native. */
c2go_extern int fcntl(int fd, int cmd, ...) {
	va_list ap;
	va_start(ap, cmd);
	if (cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW) {
		struct flock *lk = va_arg(ap, struct flock *);
		va_end(ap);
		int r = __c2go_fcntl_flock(fd, cmd, lk);
		if (r < 0) { errno = -r; return -1; }
		return r;
	}
	int arg = va_arg(ap, int);
	va_end(ap);
	int r = __c2go_syscall_fcntl(fd, cmd, arg);
	if (r < 0) { errno = -r; return -1; }
	return r;
}

c2go_extern int dup2(int oldfd, int newfd) {
	int r = __c2go_syscall_dup2(oldfd, newfd);
	if (r < 0) { errno = -r; return -1; }
	return r;
}

c2go_extern int dup3(int oldfd, int newfd, int flags) {
	int r = __c2go_syscall_dup3(oldfd, newfd, flags);
	if (r < 0) { errno = -r; return -1; }
	return r;
}

/* Path removal. remove() (source/stdio.c) is layered on unlink/rmdir. */
c2go_extern int unlink(const char *path) {
	int r = __c2go_syscall_unlink(path);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int rmdir(const char *path) {
	int r = __c2go_syscall_rmdir(path);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int rename(const char *from, const char *to) {
	int r = __c2go_syscall_rename(from, to);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

#endif /* !_WIN32 */
