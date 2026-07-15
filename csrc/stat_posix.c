/* stat_posix.c — the POSIX file-metadata layer (Unix): stat/lstat/fstat plus the
 * mutating chmod/fchmod/mkdir/access/umask.
 *
 * Same shape as source/io_posix.c: musl-style C wrappers (a POSIX return value
 * plus errno on failure) that each compose exactly one Go bridge (__c2go_syscall_*
 * in stat.go), because c2go-compiled C cannot issue a raw syscall. The scalar ops
 * follow the io.go contract verbatim: the shim returns the syscall result on
 * success or -errno on failure, and the wrapper applies musl's __syscall_ret shape
 * `if (r < 0) { errno = -r; return -1; }`.
 *
 * stat/lstat/fstat are the exception in shape but not in spirit: `struct stat` is
 * a UNIFORM c2go layout (see <sys/stat.h>), so the Go bridge fills the caller's
 * buffer field-by-field from the host stat (never a memcpy of a per-OS layout) and
 * returns 0 or -errno. The buffer pointer is handed straight to Go; the bridge
 * writes it only AFTER the blocking syscall returns, so a copystack that moves the
 * caller's frame is tracked by the Go pointer parameter (no uintptr laundering).
 *
 * umask never fails: it returns the previous mask, so its wrapper forwards the
 * bridge result directly with no errno handling.
 *
 * The whole file is empty on Windows (gen.sh globs every source for every
 * target); the Windows port LIVES in stat_windows.c (#647), following the
 * Go-first split sketched here (path family via Go + uniform cStat fill,
 * fstat via GetFileInformationByHandle).
 */
#if !defined(_WIN32)

#include <c2go.h>
#include <sys/stat.h>   /* struct stat, mode_t, stat/fstat/lstat/chmod/fchmod/mkdir/umask decls */
#include <unistd.h>     /* access decl + F_OK/R_OK/W_OK/X_OK */
#include <errno.h>      /* errno (per-OS native values) */

/* Go bridges (stat.go / stat_darwin.go / stat_linux.go). The stat trio fills the
 * uniform struct stat and returns 0 or -errno; the rest return the syscall result
 * or -errno, exactly like the io.go fd shims. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_stat", C2GO_GOABI0)
int __c2go_syscall_stat(const char *path, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_lstat", C2GO_GOABI0)
int __c2go_syscall_lstat(const char *path, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fstat", C2GO_GOABI0)
int __c2go_syscall_fstat(int fd, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_chmod", C2GO_GOABI0)
int __c2go_syscall_chmod(const char *path, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fchmod", C2GO_GOABI0)
int __c2go_syscall_fchmod(int fd, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_mkdir", C2GO_GOABI0)
int __c2go_syscall_mkdir(const char *path, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_access", C2GO_GOABI0)
int __c2go_syscall_access(const char *path, int amode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_umask", C2GO_GOABI0)
unsigned __c2go_syscall_umask(unsigned mask);

c2go_extern int stat(const char *__restrict path, struct stat *__restrict buf) {
	int r = __c2go_syscall_stat(path, buf);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int lstat(const char *__restrict path, struct stat *__restrict buf) {
	int r = __c2go_syscall_lstat(path, buf);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int fstat(int fd, struct stat *buf) {
	int r = __c2go_syscall_fstat(fd, buf);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int chmod(const char *path, mode_t mode) {
	int r = __c2go_syscall_chmod(path, mode);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int fchmod(int fd, mode_t mode) {
	int r = __c2go_syscall_fchmod(fd, mode);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int mkdir(const char *path, mode_t mode) {
	int r = __c2go_syscall_mkdir(path, mode);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int access(const char *path, int amode) {
	int r = __c2go_syscall_access(path, amode);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

/* umask cannot fail; the bridge returns the previous mask (POSIX). */
c2go_extern mode_t umask(mode_t mask) {
	return __c2go_syscall_umask(mask);
}

#endif /* !_WIN32 */
