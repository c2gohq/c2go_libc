/* err.h — BSD error-reporting family (musl include/err.h).
 * Impl source/err.c (#675). */
#ifndef _ERR_H
#define _ERR_H

#include <stdarg.h>
#include <c2go.h>

void warn(const char *, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.warn", C2GO_GOABI0);
void vwarn(const char *, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vwarn", C2GO_GOABI0);
void warnx(const char *, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.warnx", C2GO_GOABI0);
void vwarnx(const char *, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vwarnx", C2GO_GOABI0);

_Noreturn void err(int, const char *, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.err", C2GO_GOABI0);
_Noreturn void verr(int, const char *, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.verr", C2GO_GOABI0);
_Noreturn void errx(int, const char *, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.errx", C2GO_GOABI0);
_Noreturn void verrx(int, const char *, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.verrx", C2GO_GOABI0);

#endif /* _ERR_H */
