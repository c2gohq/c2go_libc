/* strings.h — case-insensitive string comparison (POSIX). The definitions live
 * in source/string.c (c2go_extern); these declarations carry c2go_linkname. */
#ifndef _STRINGS_H
#define _STRINGS_H

#define __NEED_size_t
#define __NEED_locale_t
#include <bits/alltypes.h>
#include <c2go.h>

int strcasecmp(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcasecmp", C2GO_GOABI0);
int strncasecmp(const char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strncasecmp", C2GO_GOABI0);

/* locale variants (source/string.c). One locale, so locale_t is ignored — musl's
 * own design (__strcasecmp_l just `return strcasecmp(l, r)`). */
int strcasecmp_l(const char *, const char *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcasecmp_l", C2GO_GOABI0);
int strncasecmp_l(const char *, const char *, size_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strncasecmp_l", C2GO_GOABI0);

/* ffs family — find first set bit, 1-based (0 for 0). Impl source/string.c. */
int ffs(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.ffs", C2GO_GOABI0);
int ffsl(long)
    c2go_linkname("github.com/c2gohq/c2go_libc.ffsl", C2GO_GOABI0);
int ffsll(long long)
    c2go_linkname("github.com/c2gohq/c2go_libc.ffsll", C2GO_GOABI0);

#endif /* _STRINGS_H */
