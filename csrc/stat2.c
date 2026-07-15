/* stat2.c — the time/stat thin-wrapper batch (extern-import audit
 * criterion 7: each body is one Go-shim call or a pure conversion over
 * one; self-implementing keeps both targets identical and linux-static
 * viable).
 *
 *   - utimensat: shim → unix.UtimesNanoAt (both targets; the UTIME_NOW/
 *     UTIME_OMIT sentinels are per-OS native values from <sys/stat.h> and
 *     travel unmapped).
 *   - futimens: musl spells it utimensat(fd, NULL, ...) — a NULL path is
 *     inexpressible through the Go wrappers, so the shim is per-OS
 *     (futimens_linux.go: raw SYS_UTIMENSAT with a NULL path;
 *     futimens_darwin.go: F_GETPATH recovery + UtimesNanoAt, the
 *     fdopendir/rewinddir precedent — rename race documented there).
 *   - utimes: musl legacy face — pure C µs→ns conversion over utimensat.
 *   - mkfifo: musl is mknod(mode|S_IFIFO); no mknod here, the shim is the
 *     equivalent syscall.Mkfifo (both targets).
 *   - posix_fallocate: linux-only like the musl source (fallocate(2));
 *     Apple ships no posix_fallocate either — absent on darwin (no fake
 *     stubs). POSIX shape: returns the error NUMBER, errno untouched.
 * Empty on Windows (#677 audit). */
#if !defined(_WIN32)

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_utimensat", C2GO_GOABI0)
long __c2go_syscall_utimensat(int dirfd, const char *path, const struct timespec *times, int flags);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_futimens", C2GO_GOABI0)
long __c2go_syscall_futimens(int fd, const struct timespec *times);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_mkfifo", C2GO_GOABI0)
long __c2go_syscall_mkfifo(const char *path, unsigned mode);

c2go_extern int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) {
	long r = __c2go_syscall_utimensat(dirfd, path, times, flags);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

c2go_extern int futimens(int fd, const struct timespec times[2]) {
	long r = __c2go_syscall_futimens(fd, times);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

/* musl src/linux/utimes.c reaches the same kernel entry through futimesat;
 * here the µs→ns conversion feeds utimensat directly. */
c2go_extern int utimes(const char *path, const struct timeval times[2]) {
	if (!times)
		return utimensat(AT_FDCWD, path, 0, 0);
	struct timespec ts[2];
	for (int i = 0; i < 2; i++) {
		if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000) {
			errno = EINVAL;
			return -1;
		}
		ts[i].tv_sec = times[i].tv_sec;
		ts[i].tv_nsec = times[i].tv_usec * 1000;
	}
	return utimensat(AT_FDCWD, path, ts, 0);
}

c2go_extern int mkfifo(const char *path, mode_t mode) {
	long r = __c2go_syscall_mkfifo(path, mode);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

#if defined(__linux__)
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fallocate", C2GO_GOABI0)
long __c2go_syscall_fallocate(int fd, long long off, long long len);

/* musl src/fcntl/posix_fallocate.c: fallocate(fd, 0, off, len), returning
 * the error NUMBER (POSIX: errno is NOT set). */
c2go_extern int posix_fallocate(int fd, off_t off, off_t len) {
	long r = __c2go_syscall_fallocate(fd, off, len);
	return r < 0 ? (int)-r : 0;
}
#endif /* __linux__ */

#endif /* !_WIN32 */
