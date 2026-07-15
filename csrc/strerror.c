/* strerror.c — the locale-free strerror table lookup (adapted, extracted from
 * c2go-libc's string.c blob).
 *
 * musl's strerror routes through __strerror_l (LC_MESSAGES catalog translation);
 * c2go-libc ships no message catalogs, so this indexes a single rodata message
 * blob built from the __strerror.h X-macro directly — no locale machinery. The
 * X-macro include builds one rodata message blob + an errno-indexed offset
 * table; unknown/out-of-range codes resolve to entry 0 ("No error information").
 * No static buffer -> thread-safe. strerror_r is built separately (musl
 * src/string/strerror_r.c, adapted to call this strerror). */
#include <c2go.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>   /* offsetof */
#include <locale.h>   /* locale_t (strerror_l) */

static const struct errmsgstr_t {
#define E(n, s) char str##n[sizeof(s)];
#include "__strerror.h"
#undef E
} errmsgstr = {
#define E(n, s) s,
#include "__strerror.h"
#undef E
};

static const unsigned short errmsgidx[] = {
#define E(n, s) [n] = offsetof(struct errmsgstr_t, str##n),
#include "__strerror.h"
#undef E
};

c2go_extern char *strerror(int e) {
    const char *s;
    if (e >= (int)(sizeof errmsgidx / sizeof *errmsgidx) || e < 0) e = 0;
    s = (const char *)&errmsgstr + errmsgidx[e];
    return (char *)s;
}

/* musl's __strerror_l translates the message via LC_MESSAGES; c2go-libc ships no
 * catalogs, so translation is a no-op and this forwards to strerror. */
c2go_extern char *strerror_l(int e, locale_t loc) {
    (void)loc;
    return strerror(e);
}
