/* wctype.c — wide character classification & case mapping (C.UTF-8 locale).
 *
 * A C/Go hybrid. Every class with a table-free definition is ported VERBATIM
 * from musl src/ctype: the arithmetic ranges (iswdigit/iswxdigit/iswcntrl/
 * iswprint), the White_Space list (iswspace), and the composites derived from
 * them (iswalnum/iswgraph/iswpunct/iswlower/iswupper/iswblank). iswctype/wctype
 * and towctrans/wctrans are the verbatim musl dispatch/name tables.
 *
 * Only three lookups are irreducibly table-driven in musl — iswalpha (alpha.h),
 * towlower and towupper (casemap.h). Rather than vendor musl's ~39 KB of Unicode
 * tables, those keep their ASCII path in C (the existing narrow ctype, so the
 * hot path never leaves C) and delegate ONLY the wc>=128 path to Go's `unicode`
 * package (wctype.go). This mirrors the iconv decision: a big Unicode table is
 * Go's to own, not ours to duplicate. The bridge is pure scalar (wint_t in,
 * int/wint_t out) — no pointer crosses the c2go boundary.
 *
 * Consequence: non-ASCII classification/case follows Go's Unicode version and
 * category choices, not musl's tables (e.g. iswalpha uses general category L,
 * differing from musl's Alphabetic property for a few code points such as the
 * Roman numerals U+2160+). ASCII stays POSIX-exact via the C narrow ctype.
 *
 * The _l variants ignore their locale_t (one locale). musl's __iswXXX_l /
 * weak_alias twins are dropped. wcschr comes from source/wstring.c (batch 3),
 * exported and reachable within the package. */
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <string.h>

/* ── non-ASCII Unicode lookups (wctype.go, Go's `unicode` package). Reached only
 * for wc >= 128. Scalar in/out — no pointer crosses the GoABI0 boundary. ────── */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_alpha", C2GO_GOABI0)
int __c2go_uni_alpha(wint_t wc);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_tolower", C2GO_GOABI0)
wint_t __c2go_uni_tolower(wint_t wc);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_uni_toupper", C2GO_GOABI0)
wint_t __c2go_uni_toupper(wint_t wc);

/* ── case mapping (ASCII in C, non-ASCII via Go). iswlower/iswupper derive from
 * these exactly as musl does. ─────────────────────────────────────────────── */

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

/* ── classification ────────────────────────────────────────────────────────── */

c2go_extern int iswalpha(wint_t wc)
{
	if (wc < 128) return isalpha((int)wc);
	return __c2go_uni_alpha(wc);
}

c2go_extern int iswdigit(wint_t wc)
{
	return (unsigned)wc-'0' < 10;
}

c2go_extern int iswxdigit(wint_t wc)
{
	return (unsigned)(wc-'0') < 10 || (unsigned)((wc|32)-'a') < 6;
}

c2go_extern int iswcntrl(wint_t wc)
{
	return (unsigned)wc < 32
	    || (unsigned)(wc-0x7f) < 33
	    || (unsigned)(wc-0x2028) < 2
	    || (unsigned)(wc-0xfff9) < 3;
}

c2go_extern int iswprint(wint_t wc)
{
	if (wc < 0xffU)
		return (wc+1 & 0x7f) >= 0x21;
	if (wc < 0x2028U || wc-0x202aU < 0xd800-0x202a || wc-0xe000U < 0xfff9-0xe000)
		return 1;
	if (wc-0xfffcU > 0x10ffff-0xfffc || (wc&0xfffe)==0xfffe)
		return 0;
	return 1;
}

/* Whitespace is the Unicode White_Space property minus non-breaking spaces
 * (U+00A0, U+2007, U+202F) and script-specific non-blank glyphs (U+1680,
 * U+180E) — ported verbatim from musl src/ctype/iswspace.c. */
c2go_extern int iswspace(wint_t wc)
{
	static const wchar_t spaces[] = {
		' ', '\t', '\n', '\r', 11, 12,  0x0085,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
		0x2006, 0x2008, 0x2009, 0x200a,
		0x2028, 0x2029, 0x205f, 0x3000, 0
	};
	return wc && wcschr(spaces, wc);
}

c2go_extern int iswblank(wint_t wc)
{
	return isblank((int)wc);
}

c2go_extern int iswalnum(wint_t wc)
{
	return iswdigit(wc) || iswalpha(wc);
}

c2go_extern int iswgraph(wint_t wc)
{
	/* ISO C: graph == printable and not space. */
	return !iswspace(wc) && iswprint(wc);
}

/* iswpunct — the POSIX definition (graph and not alphanumeric). musl uses a
 * dedicated punct.h table; the derivation is equivalent and needs no table, so
 * for wc>=128 it follows iswalpha (Go's unicode.Letter). */
c2go_extern int iswpunct(wint_t wc)
{
	return iswgraph(wc) && !iswalnum(wc);
}

c2go_extern int iswlower(wint_t wc)
{
	return towupper(wc) != wc;
}

c2go_extern int iswupper(wint_t wc)
{
	return towlower(wc) != wc;
}

/* ── iswctype / wctype (verbatim musl src/ctype/iswctype.c) ─────────────────── */

#define WCTYPE_ALNUM  1
#define WCTYPE_ALPHA  2
#define WCTYPE_BLANK  3
#define WCTYPE_CNTRL  4
#define WCTYPE_DIGIT  5
#define WCTYPE_GRAPH  6
#define WCTYPE_LOWER  7
#define WCTYPE_PRINT  8
#define WCTYPE_PUNCT  9
#define WCTYPE_SPACE  10
#define WCTYPE_UPPER  11
#define WCTYPE_XDIGIT 12

c2go_extern int iswctype(wint_t wc, wctype_t t)
{
	switch (t) {
	case WCTYPE_ALNUM:  return iswalnum(wc);
	case WCTYPE_ALPHA:  return iswalpha(wc);
	case WCTYPE_BLANK:  return iswblank(wc);
	case WCTYPE_CNTRL:  return iswcntrl(wc);
	case WCTYPE_DIGIT:  return iswdigit(wc);
	case WCTYPE_GRAPH:  return iswgraph(wc);
	case WCTYPE_LOWER:  return iswlower(wc);
	case WCTYPE_PRINT:  return iswprint(wc);
	case WCTYPE_PUNCT:  return iswpunct(wc);
	case WCTYPE_SPACE:  return iswspace(wc);
	case WCTYPE_UPPER:  return iswupper(wc);
	case WCTYPE_XDIGIT: return iswxdigit(wc);
	}
	return 0;
}

c2go_extern wctype_t wctype(const char *s)
{
	int i;
	const char *p;
	/* order must match the WCTYPE_* values above! */
	static const char names[] =
		"alnum\0" "alpha\0" "blank\0"
		"cntrl\0" "digit\0" "graph\0"
		"lower\0" "print\0" "punct\0"
		"space\0" "upper\0" "xdigit";
	for (i=1, p=names; *p; i++, p+=6)
		if (*s == *p && !strcmp(s, p))
			return i;
	return 0;
}

/* ── towctrans / wctrans (verbatim musl; wctrans_t is a scalar tag, see the
 * header). ─────────────────────────────────────────────────────────────────── */

c2go_extern wctrans_t wctrans(const char *class)
{
	if (!strcmp(class, "toupper")) return (wctrans_t)1;
	if (!strcmp(class, "tolower")) return (wctrans_t)2;
	return 0;
}

c2go_extern wint_t towctrans(wint_t wc, wctrans_t trans)
{
	if (trans == (wctrans_t)1) return towupper(wc);
	if (trans == (wctrans_t)2) return towlower(wc);
	return wc;
}

/* ── locale variants: one locale, so each ignores its locale_t ─────────────── */

c2go_extern int iswalnum_l(wint_t c, locale_t l)  { (void)l; return iswalnum(c); }
c2go_extern int iswalpha_l(wint_t c, locale_t l)  { (void)l; return iswalpha(c); }
c2go_extern int iswblank_l(wint_t c, locale_t l)  { (void)l; return iswblank(c); }
c2go_extern int iswcntrl_l(wint_t c, locale_t l)  { (void)l; return iswcntrl(c); }
c2go_extern int iswdigit_l(wint_t c, locale_t l)  { (void)l; return iswdigit(c); }
c2go_extern int iswgraph_l(wint_t c, locale_t l)  { (void)l; return iswgraph(c); }
c2go_extern int iswlower_l(wint_t c, locale_t l)  { (void)l; return iswlower(c); }
c2go_extern int iswprint_l(wint_t c, locale_t l)  { (void)l; return iswprint(c); }
c2go_extern int iswpunct_l(wint_t c, locale_t l)  { (void)l; return iswpunct(c); }
c2go_extern int iswspace_l(wint_t c, locale_t l)  { (void)l; return iswspace(c); }
c2go_extern int iswupper_l(wint_t c, locale_t l)  { (void)l; return iswupper(c); }
c2go_extern int iswxdigit_l(wint_t c, locale_t l) { (void)l; return iswxdigit(c); }

c2go_extern int iswctype_l(wint_t c, wctype_t t, locale_t l) { (void)l; return iswctype(c, t); }

c2go_extern wint_t towlower_l(wint_t c, locale_t l) { (void)l; return towlower(c); }
c2go_extern wint_t towupper_l(wint_t c, locale_t l) { (void)l; return towupper(c); }
c2go_extern wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l) { (void)l; return towctrans(c, t); }

c2go_extern wctype_t  wctype_l(const char *s, locale_t l)  { (void)l; return wctype(s); }
c2go_extern wctrans_t wctrans_l(const char *s, locale_t l) { (void)l; return wctrans(s); }

/* ── terminal column width (musl src/ctype/wcwidth.c + wcswidth.c) ────
 * Table-driven like alpha/casemap, but unlike those there is no Go-side
 * equivalent to delegate to (Go's unicode package has no East Asian Width
 * data), so musl's two bitmap tables (~5 KB, nonspacing.h + wide.h) are
 * vendored verbatim — they ARE the width semantics (header note). On the
 * UTF-16 target (windows) wchar_t is one UTF-16 unit: BMP code points get
 * musl's answer, and supplementary planes are outside wchar_t's domain
 * there by construction. */

static const unsigned char nonspacing_table[] = {
#include "nonspacing.h"
};

static const unsigned char wide_table[] = {
#include "wide.h"
};

c2go_extern int wcwidth(wchar_t wc)
{
	if ((unsigned)wc < 0xffU)
		return ((unsigned)wc+1 & 0x7f) >= 0x21 ? 1 : wc ? -1 : 0;
	if (((unsigned)wc & 0xfffeffffU) < 0xfffe) {
		if ((nonspacing_table[nonspacing_table[(unsigned)wc>>8]*32+(((unsigned)wc&255)>>3)]>>((unsigned)wc&7))&1)
			return 0;
		if ((wide_table[wide_table[(unsigned)wc>>8]*32+(((unsigned)wc&255)>>3)]>>((unsigned)wc&7))&1)
			return 2;
		return 1;
	}
	if (((unsigned)wc & 0xfffe) == 0xfffe)
		return -1;
	if ((unsigned)wc-0x20000U < 0x20000)
		return 2;
	if ((unsigned)wc == 0xe0001 || (unsigned)wc-0xe0020U < 0x5f || (unsigned)wc-0xe0100U < 0xef)
		return 0;
	return 1;
}

c2go_extern int wcswidth(const wchar_t *wcs, size_t n)
{
	int l=0, k=0;
	for (; n-- && *wcs && (k = wcwidth(*wcs)) >= 0; l+=k, wcs++);
	return (k < 0) ? k : l;
}
