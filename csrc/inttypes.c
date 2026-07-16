/* inttypes.c — strtoimax / strtoumax: greatest-width twins == strtoll/strtoull
 * (intmax_t is long long on every c2go target). musl aliases them onto the
 * strtol.c internals, which this libc owns inside the stdio blob (the
 * number-conversion family lives there) — so these are plain forwarders. */
#include <inttypes.h>
#include <stdlib.h>
#include <c2go.h>

c2go_extern intmax_t strtoimax(const char *nptr, char **endptr, int base) {
    return strtoll(nptr, endptr, base);
}

c2go_extern uintmax_t strtoumax(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}
