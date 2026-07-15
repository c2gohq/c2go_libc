/* io_windows.c — the Windows fd-I/O layer.
 *
 * On Unix the fd primitives are implemented directly in Go (../io.go) over Go's
 * syscall package, which owns a POSIX fd table. Windows does NOT: Go's syscall
 * package there is pure Win32 (HANDLE-based, no small-int fd table), so it cannot
 * back read(2)/open(2). The POSIX fd table on Windows lives in the C runtime
 * (msvcrt), exactly as MinGW-w64's libc uses it — so here the fd primitives
 * delegate to the CRT's _open/_read/_write/_close/_lseeki64/_dup2 (imported from
 * msvcrt.dll via the c2go unmanaged-extern bridge). dup3 and fcntl have no CRT
 * equivalent and are composed on top; close-on-exec is the CRT's _O_NOINHERIT
 * open flag (O_CLOEXEC in <bits/fcntl.h>) or SetHandleInformation on the
 * underlying HANDLE.
 *
 * The PATH operations (unlink/rmdir/rename) are the exception: they take a path,
 * not an fd, so they do NOT need the CRT fd table — Go can perform them directly.
 * Per the Go-first rule they are backed by Go (../io_windows.go) over
 * DeleteFile/RemoveDirectory/MoveFileEx rather than msvcrt, exactly as the Unix
 * fd layer routes them through io.go's syscall shims; the wrappers below just
 * apply musl's __syscall_ret shape to the shim's -errno. (This also fixes msvcrt
 * rename's non-POSIX "fails if dest exists" behaviour: MoveFileEx replaces.)
 *
 * The whole file is empty off Windows (the sources are globbed for every target
 * by gen.sh); io.go carries the Unix implementation and is //go:build unix.
 */
#if defined(_WIN32)

#include <c2go.h>
#include <unistd.h>   /* read/write/close/lseek/dup2/dup3 decls + ssize_t/off_t/size_t */
#include <fcntl.h>    /* open decl + O_* flag values (Windows = MinGW) */
#include <errno.h>    /* errno + EINVAL (per-OS native = MinGW on Windows) */
#include <stdarg.h>   /* va_list for the variadic open/fcntl */

/* msvcrt CRT fd primitives. The underscore names are msvcrt.dll's guaranteed
 * POSIX exports; the c2go-bind `-l msvcrt` step resolves them via
 * //go:cgo_import_dynamic. _dup2 returns 0 on success (NOT the new fd).
 *
 * A plain `extern` suffices: an undefined-in-package function defaults to an
 * unmanaged host import, so the `unmanaged` world keyword is redundant here. */
extern int       _open(const char *path, int oflag, int pmode);
extern int       _read(int fd, void *buf, unsigned n);
extern int       _write(int fd, const void *buf, unsigned n);
extern int       _close(int fd);
extern long long _get_osfhandle(int fd);
extern int       _open_osfhandle(long long h, int flags);
extern long long _lseeki64(int fd, long long off, int origin);
extern int       _dup2(int oldfd, int newfd);
extern int      *_errno(void);             /* &(the CRT errno) */

/* Copy the CRT errno (MinGW-native values, matching <bits/errno.h> on Windows)
 * into this goroutine's C errno after a failed CRT call. */
static void map_errno(void) { errno = *_errno(); }

/* Go path-op shims (../io_windows.go). Each returns 0 on success or -errno (the
 * MinGW value produced by winErrno's _dosmaperr reproduction); the wrappers below
 * apply musl's __syscall_ret shape — identical to the Unix io_posix.c path ops. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_unlink", C2GO_GOABI0)
int __c2go_syscall_unlink(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_rmdir", C2GO_GOABI0)
int __c2go_syscall_rmdir(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_rename", C2GO_GOABI0)
int __c2go_syscall_rename(const char *from, const char *to);

/* Virtualized std descriptors (../stdio_std.go): fds 0/1/2 denote the LIVE
 * os.Stdin/Stdout/Stderr, NOT CRT fds. On unix this routing lives in the Go
 * __c2go_syscall_* shims; here the CRT wrappers branch in C — same portable
 * std shims either way. See source/stdio.c's std-streams section. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_read", C2GO_GOABI0)
long long __c2go_std_read(int which, void *buf, unsigned long long n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_write", C2GO_GOABI0)
long long __c2go_std_write(int which, const void *buf, unsigned long long n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_seek", C2GO_GOABI0)
long long __c2go_std_seek(int which, long long off, int whence);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_isdefault", C2GO_GOABI0)
int __c2go_std_isdefault(int which);

c2go_extern ssize_t read(int fd, void *buf, size_t n) {
	if (fd >= 0 && fd <= 2) {
		long long r = __c2go_std_read(fd, buf, n);
		if (r < 0) { errno = (int)-r; return -1; }
		return (ssize_t)r;
	}
	if (n > 0x7fffffff) n = 0x7fffffff; /* CRT counts are int (#661): clamp, don't truncate */
	int r = _read(fd, buf, (unsigned)n);
	if (r < 0) map_errno();
	return r;
}

c2go_extern ssize_t write(int fd, const void *buf, size_t n) {
	if (fd >= 0 && fd <= 2) {
		long long r = __c2go_std_write(fd, buf, n);
		if (r < 0) { errno = (int)-r; return -1; }
		return (ssize_t)r;
	}
	if (n > 0x7fffffff) n = 0x7fffffff; /* #661 */
	int r = _write(fd, buf, (unsigned)n);
	if (r < 0) map_errno();
	return r;
}

/* open is variadic in C; c2go lowers the vararg mode through the void** pack, so
 * va_arg here reads argptrs[0] (exactly as the printf/scanf family does). O_BINARY
 * is forced so ported byte I/O is not corrupted by the CRT's CRLF translation;
 * close-on-exec rides in via O_CLOEXEC == _O_NOINHERIT (an open flag on Windows). */
c2go_extern int open(const char *path, int flags, ...) {
	int pmode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		pmode = va_arg(ap, int);
		va_end(ap);
	}
	int fd = _open(path, flags | O_BINARY, pmode);
	if (fd < 0) map_errno();
	return fd;
}

c2go_extern int close(int fd) {
	/* CRT fds 0/1/2 back the same console HANDLEs Go's os.Std* wrap (the std
	 * streams route through the live os.Std* — see stdio.c); _close(1) would
	 * close that handle out from under Go. Refuse, mirroring io_posix.c. */
	if (fd >= 0 && fd <= 2) { errno = EBADF; return -1; }
	int r = _close(fd);
	if (r < 0) map_errno();
	return r;
}

c2go_extern off_t lseek(int fd, off_t off, int whence) {
	if (fd >= 0 && fd <= 2) {
		long long r = __c2go_std_seek(fd, (long long)off, whence);
		if (r < 0) { errno = (int)-r; return -1; }
		return (off_t)r;
	}
	long long r = _lseeki64(fd, (long long)off, whence);
	if (r < 0) map_errno();
	return (off_t)r;
}

/* POSIX dup2 returns newfd; the CRT's _dup2 returns 0 on success. No dup3:
 * Windows / MinGW-w64 has none (it is a Linux syscall), and its only extra over
 * dup2 — an atomic close-on-exec on the copy — is unexpressible on a CRT fd; the
 * ported freopen uses dup2 on Windows (see source/stdio.c). Std descriptors
 * (#658 M14, unix parity): dup2 onto 0/1/2 rebinds the live os.Std* through
 * the #644 freopen machinery; dup2 FROM a std slot materializes the live
 * handle as a CRT fd first. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_dup_handle", C2GO_GOABI0)
long long __c2go_std_dup_handle(int which);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_rebind", C2GO_GOABI0)
int __c2go_std_rebind(int which, long long h);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_closehandle", C2GO_GOABI0)
void __c2go_closehandle(long long h);

c2go_extern int dup2(int oldfd, int newfd) {
	int old_std = (oldfd >= 0 && oldfd <= 2), new_std = (newfd >= 0 && newfd <= 2);
	if (oldfd == newfd && old_std) return newfd; /* std slots are always live */
	if (new_std) {
		/* Redirect a std slot (#658 M14, unix stdDupGate parity): rebind the
		 * live os.Std* to a duplicate of oldfd's handle — freopen(std)'s #644
		 * machinery. __c2go_std_rebind duplicates internally, so the handle we
		 * pass is only borrowed (a std OLDFD lends an owned duplicate that is
		 * closed after; a CRT fd lends _get_osfhandle's view). */
		long long h;
		int owned = 0;
		if (old_std) {
			h = __c2go_std_dup_handle(oldfd);
			if (h < 0) { errno = (int)-h; return -1; }
			owned = 1;
		} else {
			h = _get_osfhandle(oldfd);
			if (h == -1) { errno = EBADF; return -1; }
		}
		int r = __c2go_std_rebind(newfd, h);
		if (owned) __c2go_closehandle(h);
		if (r < 0) { errno = -r; return -1; }
		return newfd;
	}
	if (old_std) {
		/* dup2(std, n>=3): materialize the live handle as a CRT fd, _dup2 it
		 * onto n, drop the temporary. */
		long long h = __c2go_std_dup_handle(oldfd);
		if (h < 0) { errno = (int)-h; return -1; }
		int tmp = _open_osfhandle(h, O_BINARY);
		if (tmp < 0) { __c2go_closehandle(h); map_errno(); return -1; }
		if (tmp == newfd) return newfd; /* CRT handed us the target slot */
		int r = _dup2(tmp, newfd);
		_close(tmp);
		if (r != 0) { map_errno(); return -1; }
		return newfd;
	}
	if (_dup2(oldfd, newfd) != 0) { map_errno(); return -1; }
	return newfd;
}

/* No fcntl: Windows / the msvcrt CRT has none (MinGW-w64 provides none either),
 * and a CRT fd's descriptor/status flags cannot change after open. The ported
 * stdio sets close-on-exec / append at open() time and elides fcntl (its only
 * remaining use, freopen(NULL,…), is rejected on Windows — see source/stdio.c). */

/* Path removal — Go-backed (../io_windows.go), applying musl's __syscall_ret
 * shape to the shim's -errno. remove() (source/stdio.c) is layered on unlink/
 * rmdir; DeleteFile fails a directory with ERROR_ACCESS_DENIED -> EACCES (not
 * EISDIR), so remove() stays file-only on Windows, matching the prior CRT
 * behaviour. */
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

/* rename via Go's MoveFileEx(MOVEFILE_REPLACE_EXISTING) — the POSIX atomic
 * replace the msvcrt CRT rename lacks (it fails if the destination exists). */
c2go_extern int rename(const char *from, const char *to) {
	int r = __c2go_syscall_rename(from, to);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

#endif /* _WIN32 */
