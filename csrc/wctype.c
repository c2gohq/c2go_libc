/* wctype.c — the ADAPTED wide-ctype: the case/alpha lookups whose big Unicode
 * tables are delegated to Go, plus iswpunct (derived, not musl's punct.h table).
 *
 * The verbatim-musl classifiers (iswdigit/iswspace/iswcntrl/iswprint/iswxdigit/
 * iswblank/iswalnum/iswgraph/iswlower/iswupper, iswctype/wctype, wctrans/
 * towctrans) are musl's algorithm unchanged and are built from the musl fork
 * (src/ctype/*.c). They call the functions below across the musl/csrc directory
 * split -- that split is provenance only (musl-derived vs our own); both are the
 * same package / same layer, so the cross-directory calls are ordinary calls.
 *
 * KEPT here because the body is OUR adaptation, not musl's:
 *   - iswalpha / towlower / towupper: musl's alpha.h/casemap.h (~39KB) are NOT
 *     vendored; the ASCII path stays in C (narrow ctype) and only the wc>=128
 *     path is delegated to Go's `unicode` package (wctype.go) -- a big Unicode
 *     table is Go's to own (mirrors iconv). Non-ASCII classification/case then
 *     follows Go's Unicode version and categories, not musl's tables.
 *   - iswpunct: musl uses a dedicated punct.h table; we derive it (graph and not
 *     alnum) so that for wc>=128 it stays consistent with the Go-based iswalpha.
 *
 * The _l variants ignore their locale_t (one locale). The bridge is pure scalar
 * (wint_t in, int/wint_t out) -- no pointer crosses the c2go boundary. */
#include <wctype.h>
#include <ctype.h>
#include <c2go.h>

/* ── non-ASCII Unicode lookups (wctype.go, Go's `unicode` package). Reached only
 * for wc >= 128. Scalar in/out — no pointer crosses the GoABI0 boundary. ────── */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_alpha", C2GO_GOABI0)
int __c2go_uni_alpha(wint_t wc);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_tolower", C2GO_GOABI0)
wint_t __c2go_uni_tolower(wint_t wc);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_toupper", C2GO_GOABI0)
wint_t __c2go_uni_toupper(wint_t wc);

/* ── case mapping (ASCII in C, non-ASCII via Go). musl's iswlower/iswupper —
 * built from the fork — derive from these exactly as musl does. ────────────── */

c2go_extern wint_t towlower(wint_t wc)
{
	if (wc < 128) return (wint_t)tolower((int)wc);
	return __c2go_uni_tolower(wc);
}

c2go_extern wint_t towupper(wint_t wc)
{
	if (wc < 128) return (wint_t)toupper((int)wc);
	return __c2go_uni_toupper(wc);
}

c2go_extern int iswalpha(wint_t wc)
{
	if (wc < 128) return isalpha((int)wc);
	return __c2go_uni_alpha(wc);
}

/* iswpunct — the POSIX definition (graph and not alphanumeric). musl uses a
 * dedicated punct.h table; the derivation is equivalent and needs no table, so
 * for wc>=128 it follows iswalpha (Go's unicode.Letter). iswgraph/iswalnum are
 * built from the musl fork. */
c2go_extern int iswpunct(wint_t wc)
{
	return iswgraph(wc) && !iswalnum(wc);
}

/* ── locale variants for the adapted lookups (one locale, so ignore locale_t) ── */
c2go_extern int iswalpha_l(wint_t c, locale_t l)  { (void)l; return iswalpha(c); }
c2go_extern int iswpunct_l(wint_t c, locale_t l)  { (void)l; return iswpunct(c); }
c2go_extern wint_t towlower_l(wint_t c, locale_t l) { (void)l; return towlower(c); }
c2go_extern wint_t towupper_l(wint_t c, locale_t l) { (void)l; return towupper(c); }
