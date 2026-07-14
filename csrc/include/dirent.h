/* dirent.h — directory-stream iteration (POSIX). opendir/readdir/... are served
 * by a Go bridge (dirent.go: os.File.ReadDir over the shared handle table); the
 * DIR handle + returned-entry buffer live in source/dirent.c. Cross-platform
 * since #678 (MinGW ships dirent.h; the enumeration primitive is platform-
 * independent). dirfd/fdopendir stay unix-only: windows' os.File.Fd() is a
 * HANDLE, not a CRT fd — a "directory fd" cannot be honestly delivered. */
#ifndef _DIRENT_H
#define _DIRENT_H

#include <c2go.h>
#define __NEED_ino_t
#define __NEED_off_t
#define __NEED_size_t
#include <bits/alltypes.h>

/* d_type values */
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

struct dirent {
	ino_t          d_ino;
	off_t          d_off;
	unsigned short d_reclen;
	unsigned char  d_type;
	char           d_name[256];
};

typedef struct __dirstream DIR;

DIR *opendir(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.opendir", C2GO_GOABI0);
struct dirent *readdir(DIR *)
    c2go_linkname("github.com/c2gohq/c2go_libc.readdir", C2GO_GOABI0);
int readdir_r(DIR *__restrict, struct dirent *__restrict, struct dirent **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.readdir_r", C2GO_GOABI0);
int closedir(DIR *)
    c2go_linkname("github.com/c2gohq/c2go_libc.closedir", C2GO_GOABI0);
void rewinddir(DIR *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rewinddir", C2GO_GOABI0);
/* scandir + comparators (source/scandir.c). Cross-platform. sel/cmp are called
 * FROM C (cmp through qsort), so they must be internal-ABI functions in the
 * CALLER's own translation unit — a plain c2go-compiled C function, exactly
 * like a qsort comparator. alphasort/versionsort below are GoABI0 boundary
 * exports — a different calling convention than the internal-CC cmp slot, so
 * one cannot be passed straight as cmp; wrap it in a local comparator, e.g.
 * `int cmp(const struct dirent **a, const struct dirent **b){ return
 * alphasort(a,b); }` (a normal boundary call from an internal fn). NOTE: this
 * is NOT the c2go_callout case — callout's operand must be an unmanaged host
 * import, and a c2go_linkname export like alphasort is explicitly rejected. */
int scandir(const char *, struct dirent ***,
            int (*)(const struct dirent *),
            int (*)(const struct dirent **, const struct dirent **))
    c2go_linkname("github.com/c2gohq/c2go_libc.scandir", C2GO_GOABI0);
int alphasort(const struct dirent **, const struct dirent **)
    c2go_linkname("github.com/c2gohq/c2go_libc.alphasort", C2GO_GOABI0);
int versionsort(const struct dirent **, const struct dirent **)
    c2go_linkname("github.com/c2gohq/c2go_libc.versionsort", C2GO_GOABI0);
#if !defined(_WIN32)
int dirfd(DIR *)   /* #675; unix-only (HANDLE != CRT fd, #677) */
    c2go_linkname("github.com/c2gohq/c2go_libc.dirfd", C2GO_GOABI0);
DIR *fdopendir(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fdopendir", C2GO_GOABI0);
#endif

#endif /* _DIRENT_H */
