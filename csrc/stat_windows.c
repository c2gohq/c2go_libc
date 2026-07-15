/* stat_windows.c — the Windows file-metadata layer (#647), the per-OS
 * counterpart of stat_posix.c. Go-first per that file's plan: stat/lstat/
 * chmod/mkdir/access take a PATH, so the Go bridges (../stat_windows.go —
 * os.Stat/os.Lstat/os.Chmod/os.Mkdir) serve them and fill the UNIFORM c2go
 * struct stat field-by-field (never a per-OS layout copy). fstat is fd-bound:
 * the wrapper recovers the WIN32 HANDLE via _get_osfhandle and the bridge
 * fills from GetFileInformationByHandle; the virtualized std descriptors
 * (0/1/2) fill from the live os.Std* object's Stat().
 *
 * umask goes through the CRT's _umask — the CRT owns the process umask that
 * its _open honours. fchmod is NOT provided: MinGW-w64 has none, and a fake
 * stub would lie; <sys/stat.h> guards the declaration off on Windows.
 *
 * The whole file is empty off Windows. */
#if defined(_WIN32)

#include <c2go.h>
#include <sys/stat.h>   /* struct stat, mode_t, the decls */
#include <unistd.h>     /* access decl + F_OK/R_OK/W_OK/X_OK */
#include <errno.h>      /* errno (MinGW-native values) */

/* msvcrt imports. */
extern int       _umask(int mode);
extern long long _get_osfhandle(int fd);
extern int      *_errno(void);

static void map_errno_st(void) { errno = *_errno(); }

/* Go bridges (../stat_windows.go). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_stat", C2GO_GOABI0)
int __c2go_syscall_stat(const char *path, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_lstat", C2GO_GOABI0)
int __c2go_syscall_lstat(const char *path, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_fstat_h", C2GO_GOABI0)
int __c2go_syscall_fstat_h(long long h, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_std_fstat_win", C2GO_GOABI0)
int __c2go_std_fstat_win(int which, struct stat *buf);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_chmod", C2GO_GOABI0)
int __c2go_syscall_chmod(const char *path, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_mkdir", C2GO_GOABI0)
int __c2go_syscall_mkdir(const char *path, unsigned mode);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_access", C2GO_GOABI0)
int __c2go_syscall_access(const char *path, int amode);

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
	int r;
	if (fd >= 0 && fd <= 2) {
		r = __c2go_std_fstat_win(fd, buf);   /* virtualized: live os.Std* */
	} else {
		long long h = _get_osfhandle(fd);
		if (h == -1) { map_errno_st(); return -1; }
		r = __c2go_syscall_fstat_h(h, buf);
	}
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern int chmod(const char *path, mode_t mode) {
	int r = __c2go_syscall_chmod(path, mode);
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

/* umask cannot fail; the CRT returns the previous mask. */
c2go_extern mode_t umask(mode_t mask) {
	return (mode_t)_umask((int)mask);
}

#endif /* _WIN32 */
