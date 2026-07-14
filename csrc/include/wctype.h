/* wctype.h — wide character classification & case mapping (C.UTF-8 locale).
 *
 * c2go-libc has one locale (C.UTF-8), so the _l variants ignore their locale_t
 * and behave as the C-locale functions. Classification/mapping is a C/Go hybrid
 * (source/wctype.c + wctype.go): the table-free classes (arithmetic ranges and
 * POSIX derivations) and the ASCII fast path of every function are C, ported
 * from musl src/ctype; only the three irreducibly table-driven NON-ASCII lookups
 * — iswalpha / towlower / towupper — delegate to Go's `unicode` package, so no
 * large Unicode table is vendored into this libc (mirroring the iconv decision:
 * a big Unicode table is Go's to own, not ours to duplicate).
 *
 * Each definition is c2go_extern (the export authority); every declaration here
 * carries the matching c2go_linkname (the CC-consistency rule) naming the Go
 * symbol + the GoABI0 boundary CC. */
#ifndef _WCTYPE_H
#define _WCTYPE_H

#include <c2go.h>        /* the c2go_linkname macro (REQUIRED before first use) */

#define __NEED_wint_t
#define __NEED_wchar_t
#define __NEED_locale_t
#include <bits/alltypes.h>

typedef unsigned long wctype_t;

/* POSIX allows wctrans_t to be any scalar type. musl types it as `const int *`
 * and stores a fabricated 1/2 tag in it; under c2go that fake pointer would be
 * traced by the GC as a heap reference (a crash on address 0x1/0x2), so
 * c2go-libc uses a plain scalar. The tag values (1=toupper, 2=tolower) are
 * unchanged — see source/wctype.c. */
typedef int wctrans_t;

/* ── classification ────────────────────────────────────────────────────────── */
int iswalnum(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswalnum", C2GO_GOABI0);
int iswalpha(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswalpha", C2GO_GOABI0);
int iswblank(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswblank", C2GO_GOABI0);
int iswcntrl(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswcntrl", C2GO_GOABI0);
int iswdigit(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswdigit", C2GO_GOABI0);
int iswgraph(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswgraph", C2GO_GOABI0);
int iswlower(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswlower", C2GO_GOABI0);
int iswprint(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswprint", C2GO_GOABI0);
int iswpunct(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswpunct", C2GO_GOABI0);
int iswspace(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswspace", C2GO_GOABI0);
int iswupper(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswupper", C2GO_GOABI0);
int iswxdigit(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswxdigit", C2GO_GOABI0);
int iswctype(wint_t, wctype_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswctype", C2GO_GOABI0);

/* ── case mapping / transforms ─────────────────────────────────────────────── */
wint_t towlower(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towlower", C2GO_GOABI0);
wint_t towupper(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towupper", C2GO_GOABI0);
wint_t towctrans(wint_t, wctrans_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towctrans", C2GO_GOABI0);

/* ── name lookups ──────────────────────────────────────────────────────────── */
wctype_t  wctype(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctype", C2GO_GOABI0);
wctrans_t wctrans(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctrans", C2GO_GOABI0);

/* ── locale variants: one locale, so each ignores its locale_t ─────────────── */
int iswalnum_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswalnum_l", C2GO_GOABI0);
int iswalpha_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswalpha_l", C2GO_GOABI0);
int iswblank_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswblank_l", C2GO_GOABI0);
int iswcntrl_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswcntrl_l", C2GO_GOABI0);
int iswdigit_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswdigit_l", C2GO_GOABI0);
int iswgraph_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswgraph_l", C2GO_GOABI0);
int iswlower_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswlower_l", C2GO_GOABI0);
int iswprint_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswprint_l", C2GO_GOABI0);
int iswpunct_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswpunct_l", C2GO_GOABI0);
int iswspace_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswspace_l", C2GO_GOABI0);
int iswupper_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswupper_l", C2GO_GOABI0);
int iswxdigit_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswxdigit_l", C2GO_GOABI0);
int iswctype_l(wint_t, wctype_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.iswctype_l", C2GO_GOABI0);
wint_t towlower_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towlower_l", C2GO_GOABI0);
wint_t towupper_l(wint_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towupper_l", C2GO_GOABI0);
wint_t towctrans_l(wint_t, wctrans_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.towctrans_l", C2GO_GOABI0);
wctype_t  wctype_l(const char *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctype_l", C2GO_GOABI0);
wctrans_t wctrans_l(const char *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctrans_l", C2GO_GOABI0);

#endif /* _WCTYPE_H */
