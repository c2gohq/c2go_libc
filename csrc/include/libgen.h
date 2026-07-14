/* libgen.h — POSIX basename/dirname (musl include/libgen.h; the POSIX forms,
 * which may modify their argument). Impl source/misc.c (#675). */
#ifndef _LIBGEN_H
#define _LIBGEN_H

#include <c2go.h>

char *basename(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.basename", C2GO_GOABI0);
char *dirname(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.dirname", C2GO_GOABI0);

#endif /* _LIBGEN_H */
