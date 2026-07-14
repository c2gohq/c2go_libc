/* iconv.h — POSIX codeset conversion.
 *
 * iconv is a Go bridge over golang.org/x/text/encoding (iconv.go), reached
 * through thin C wrappers (source/iconv.c) that own the (iconv_t)-1 failure
 * sentinel: the Go side deals only in real pointers (the descriptor is the
 * rooted *iconvState itself; failure is nil), because a manufactured
 * non-pointer value in a Go pointer slot trips the precise stack scan. */
#ifndef _ICONV_H
#define _ICONV_H

#include <c2go.h>

#define __NEED_size_t
#include <bits/alltypes.h>

typedef void *iconv_t;

c2go_linkname("github.com/c2gohq/c2go_libc.iconv_open", C2GO_GOABI0)
iconv_t iconv_open(const char *, const char *);

c2go_linkname("github.com/c2gohq/c2go_libc.iconv", C2GO_GOABI0)
size_t iconv(iconv_t, char **__restrict, size_t *__restrict,
             char **__restrict, size_t *__restrict);

c2go_linkname("github.com/c2gohq/c2go_libc.iconv_close", C2GO_GOABI0)
int iconv_close(iconv_t);

#endif /* _ICONV_H */
