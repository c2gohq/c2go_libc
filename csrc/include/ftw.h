/* ftw.h — file tree walk (musl include/ftw.h, #675 C wave 2b). Impl
 * source/nftw.c (musl verbatim; ftw() is the musl src/legacy one-liner).
 * Feature gating and the _LARGEFILE64_SOURCE aliases are dropped (this libc
 * ships the full surface; off_t is always 64-bit). */
#ifndef _FTW_H
#define _FTW_H

#if defined(_WIN32)
#error "<ftw.h> is not available on Windows (no MinGW/CRT counterpart; #677 audit)"
#endif

#include <c2go.h>
#include <sys/stat.h>

#define FTW_F   1
#define FTW_D   2
#define FTW_DNR 3
#define FTW_NS  4
#define FTW_SL  5
#define FTW_DP  6
#define FTW_SLN 7

#define FTW_PHYS  1
#define FTW_MOUNT 2
#define FTW_CHDIR 4
#define FTW_DEPTH 8

struct FTW {
	int base;
	int level;
};

int ftw(const char *, int (*)(const char *, const struct stat *, int), int)
    c2go_linkname("github.com/c2gohq/c2go_libc.ftw", C2GO_GOABI0);
int nftw(const char *, int (*)(const char *, const struct stat *, int, struct FTW *), int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.nftw", C2GO_GOABI0);

#endif /* _FTW_H */
