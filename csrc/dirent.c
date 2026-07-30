/* dirent.c — directory streams (cross-platform since #678: MinGW ships
 * dirent.h and the enumeration is os.File.ReadDir, platform-independent).
 * The DIR object holds a Go directory-stream handle (dirent.go: an *os.File
 * in the shared handle table) plus the struct dirent buffer readdir returns
 * a pointer to. The entry buffer is passed to the bridge as void* (filled
 * through an unsafe.Pointer cast on the Go side, the stat-bridge shape).
 * dirfd/fdopendir stay unix-only: on windows os.File.Fd() is a HANDLE, not
 * a CRT fd — a "directory fd" cannot be honestly delivered (#677). */

#include <c2go.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>   /* malloc / free */

struct __dirstream {
	/* Internal Go handle-table ids have a fixed 64-bit ABI.  Do not use C
	 * long here: it is only 32 bits on Windows LLP64. */
	long long handle;   /* dirent.go handle-table id (>= 1) */
	struct dirent de;   /* the entry readdir returns a pointer to */
};

/* Go bridges (dirent.go). */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_opendir", C2GO_GOABI0)
long long __c2go_opendir(const char *path);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_readdir", C2GO_GOABI0)
int __c2go_readdir(long long handle, void *de);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_closedir", C2GO_GOABI0)
int __c2go_closedir(long long handle);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_rewinddir", C2GO_GOABI0)
int __c2go_rewinddir(long long handle);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_dirfd", C2GO_GOABI0)
long long __c2go_dirfd(long long handle);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_fdopendir", C2GO_GOABI0)
long long __c2go_fdopendir(int fd);

c2go_extern DIR *opendir(const char *name)
{
	long long h = __c2go_opendir(name);
	if (h < 0) { errno = (int)-h; return (void *)0; }
	DIR *d = malloc(sizeof *d);
	if (!d) { __c2go_closedir(h); errno = ENOMEM; return (void *)0; }
	d->handle = h;
	return d;
}

/* dirfd/fdopendir (#675, nftw's walk primitives) — unix-only, see header. */
#if !defined(_WIN32)
c2go_extern int dirfd(DIR *d)
{
	long long r = __c2go_dirfd(d->handle);
	if (r < 0) { errno = (int)-r; return -1; }
	return (int)r;
}

c2go_extern DIR *fdopendir(int fd)
{
	long long h = __c2go_fdopendir(fd);
	if (h < 0) { errno = (int)-h; return (void *)0; }
	DIR *d = malloc(sizeof *d);
	if (!d) { __c2go_closedir(h); errno = ENOMEM; return (void *)0; }
	d->handle = h;
	return d;
}
#endif /* !_WIN32 (dirfd/fdopendir) */

/* readdir returns a pointer to the DIR's own entry buffer, or NULL at end of
 * directory (errno unchanged) / on error (errno set). */
c2go_extern struct dirent *readdir(DIR *d)
{
	int r = __c2go_readdir(d->handle, &d->de);
	if (r <= 0) {
		if (r < 0) errno = -r;
		return (void *)0;
	}
	return &d->de;
}

/* readdir_r (deprecated but still used): fill the caller's entry; *result points
 * at it, or is NULL at end of directory. Returns 0 on success, a positive errno
 * on error (POSIX). */
c2go_extern int readdir_r(DIR *restrict d, struct dirent *restrict entry,
                          struct dirent **restrict result)
{
	int r = __c2go_readdir(d->handle, entry);
	if (r < 0) { *result = (void *)0; return -r; }
	*result = r ? entry : (void *)0;
	return 0;
}

c2go_extern int closedir(DIR *d)
{
	int r = __c2go_closedir(d->handle);
	free(d);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}

c2go_extern void rewinddir(DIR *d)
{
	__c2go_rewinddir(d->handle);
}
