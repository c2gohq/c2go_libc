/* fnmatch.h — POSIX filename pattern matching (musl include/fnmatch.h).
 * Impl source/fnmatch.c (#667). */
#ifndef _FNMATCH_H
#define _FNMATCH_H

#include <c2go.h>

#define	FNM_PATHNAME 0x1
#define	FNM_NOESCAPE 0x2
#define	FNM_PERIOD   0x4
#define	FNM_LEADING_DIR	0x8
#define	FNM_CASEFOLD	0x10
#define	FNM_FILE_NAME	FNM_PATHNAME

#define	FNM_NOMATCH 1
#define FNM_NOSYS   (-1)

int fnmatch(const char *, const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fnmatch", C2GO_GOABI0);

#endif /* _FNMATCH_H */
