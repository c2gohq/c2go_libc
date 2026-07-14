/* monetary.h — strfmon (source/locale.c, musl src/locale/strfmon.c). Only the
 * C locale exists, and the C locale defines no currency symbol, grouping, or
 * sign layout, so the flags/symbol machinery reduces to musl's own C-locale
 * behavior: plain "%*.*f" numeric formatting. */
#ifndef _MONETARY_H
#define _MONETARY_H

#include <c2go.h>
#include <locale.h>   /* locale_t for strfmon_l */

#define __NEED_size_t
#define __NEED_ssize_t
#include <bits/alltypes.h>

ssize_t strfmon(char *__restrict, size_t, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.strfmon", C2GO_GOABI0);
ssize_t strfmon_l(char *__restrict, size_t, locale_t, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.strfmon_l", C2GO_GOABI0);

#endif /* _MONETARY_H */
