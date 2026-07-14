/* uchar.h — C11 Unicode character utilities (musl include/uchar.h).
 * Impl source/uchar.c (#673). */
#ifndef _UCHAR_H
#define _UCHAR_H

#include <c2go.h>

typedef unsigned short char16_t;
typedef unsigned char32_t;

#define __NEED_mbstate_t
#define __NEED_size_t

#include <bits/alltypes.h>

size_t c16rtomb(char *__restrict, char16_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.c16rtomb", C2GO_GOABI0);
size_t mbrtoc16(char16_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbrtoc16", C2GO_GOABI0);

size_t c32rtomb(char *__restrict, char32_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.c32rtomb", C2GO_GOABI0);
size_t mbrtoc32(char32_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbrtoc32", C2GO_GOABI0);

#endif /* _UCHAR_H */
