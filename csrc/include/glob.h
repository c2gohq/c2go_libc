/* glob.h — POSIX pathname pattern expansion (musl include/glob.h).
 * Impl source/glob.c (#667). */
#ifndef	_GLOB_H
#define	_GLOB_H

#include <c2go.h>

#define __NEED_size_t

#include <bits/alltypes.h>

#ifndef C2GO_GLOB_OMIT_TYPE
typedef struct {
	size_t gl_pathc;
	char **gl_pathv;
	size_t gl_offs;
	int __dummy1;
	void *__dummy2[5];
} glob_t;
#endif

/* Cross-platform since #678: the dirent walk layer is (MinGW ships dirent.h;
 * our enumeration is os.File.ReadDir), and stat/fnmatch already are. The `~`
 * expansion uses $HOME — commonly unset on windows, where it degrades to
 * GLOB_NOMATCH (musl's own tilde-failure path). */
#ifndef C2GO_GLOB_OMIT_FUNCTIONS
int glob(const char *__restrict, int, int (*)(const char *, int), glob_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.glob", C2GO_GOABI0);
void globfree(glob_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.globfree", C2GO_GOABI0);
#endif

#define GLOB_ERR      0x01
#define GLOB_MARK     0x02
#define GLOB_NOSORT   0x04
#define GLOB_DOOFFS   0x08
#define GLOB_NOCHECK  0x10
#define GLOB_APPEND   0x20
#define GLOB_NOESCAPE 0x40
#define	GLOB_PERIOD   0x80

#define GLOB_TILDE       0x1000
#define GLOB_TILDE_CHECK 0x4000

#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3
#define GLOB_NOSYS   4

#endif /* _GLOB_H */
