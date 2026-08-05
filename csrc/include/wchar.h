/* wchar.h — restartable multibyte <-> wide conversions (UTF-8 <-> wchar_t).
 *
 * wchar_t is int32 (UTF-32) on the unix targets and uint16 (UTF-16) on Windows,
 * where a supplementary scalar travels as a surrogate PAIR (multibyte.c + the
 * stdio.c wide I/O). This header declares the restartable multibyte conversions
 * (source/multibyte.c), the wcs/wmem wide string/memory family (source/wstring.c),
 * the wide numeric parsers wcstod/wcstol, the wide FILE I/O family
 * fputwc/fgetwc/fwide/... and the wprintf/wscanf wide FORMATTED-I/O family (both
 * source/stdio.c). Each definition is c2go_extern;
 * every declaration here carries a matching c2go_linkname (the CC-consistency
 * rule) naming the Go symbol (lowercase C name) + the ABI0 CC. */
#ifndef _WCHAR_H
#define _WCHAR_H

#include <c2go.h>        /* the c2go_linkname macro (REQUIRED before first use) */

#define __NEED_wchar_t
#define __NEED_wint_t
#define __NEED_mbstate_t
#define __NEED_size_t
#ifndef C2GO_MLIB_FILE_REPLACEMENT
#define __NEED_FILE
#endif
#define __NEED_va_list
#define __NEED_locale_t
#include <bits/alltypes.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define WEOF (0xffffffffU)

/* single-character (restartable) */
size_t mbrtowc(wchar_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbrtowc", C2GO_GOABI0);
/* internal: restartable UTF-8 -> 32-bit scalar decode (the C11 mbrtoc32 core,
 * writing the full scalar, never surrogate-splitting). Shared by multibyte.c's
 * mbrtowc/string engines and stdio.c's wide FILE I/O (which splits supplementary
 * scalars into surrogate pairs on UTF-16). */
size_t __mbrtoc32(unsigned *__restrict, const char *__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.__mbrtoc32", C2GO_GOABI0);
/* wcrtomb is musl VERBATIM in the musl fork (src/multibyte/wcrtomb.c); no 32-bit
 * encode core -- a wchar_t-width value can't be supplementary. (decode differs:
 * __mbrtoc32 above is the shared 32-bit decode core.) */
size_t wcrtomb(char *__restrict, wchar_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcrtomb", C2GO_GOABI0);
size_t mbrlen(const char *__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbrlen", C2GO_GOABI0);
int    mbsinit(const mbstate_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbsinit", C2GO_GOABI0);

/* single-byte <-> wide */
wint_t btowc(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.btowc", C2GO_GOABI0);
int    wctob(wint_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctob", C2GO_GOABI0);

/* whole-string (restartable) */
size_t mbsrtowcs(wchar_t *__restrict, const char **__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbsrtowcs", C2GO_GOABI0);
size_t wcsrtombs(char *__restrict, const wchar_t **__restrict, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsrtombs", C2GO_GOABI0);
size_t mbsnrtowcs(wchar_t *__restrict, const char **__restrict, size_t, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbsnrtowcs", C2GO_GOABI0);
size_t wcsnrtombs(char *__restrict, const wchar_t **__restrict, size_t, size_t, mbstate_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsnrtombs", C2GO_GOABI0);

/* wide string / wide memory (source/wstring.c, ported verbatim from musl's
 * src/string wcs / wmem family). Pure element-wise algorithms over wchar_t
 * arrays — cross-platform. wcscasecmp/wcsncasecmp case-fold with towlower
 * (source/wctype.c). Each c2go_extern definition needs its matching linkname. */

/* length */
size_t   wcslen(const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcslen", C2GO_GOABI0);
size_t   wcsnlen(const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsnlen", C2GO_GOABI0);

/* copy */
wchar_t *wcscpy(wchar_t *__restrict, const wchar_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscpy", C2GO_GOABI0);
wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcpcpy", C2GO_GOABI0);
wchar_t *wcsncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsncpy", C2GO_GOABI0);
wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcpncpy", C2GO_GOABI0);

/* concatenate */
wchar_t *wcscat(wchar_t *__restrict, const wchar_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscat", C2GO_GOABI0);
wchar_t *wcsncat(wchar_t *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsncat", C2GO_GOABI0);

/* compare */
int      wcscmp(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscmp", C2GO_GOABI0);
int      wcsncmp(const wchar_t *, const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsncmp", C2GO_GOABI0);
int      wcscasecmp(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscasecmp", C2GO_GOABI0);
int      wcsncasecmp(const wchar_t *, const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsncasecmp", C2GO_GOABI0);
int      wcscasecmp_l(const wchar_t *, const wchar_t *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscasecmp_l", C2GO_GOABI0);
int      wcsncasecmp_l(const wchar_t *, const wchar_t *, size_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsncasecmp_l", C2GO_GOABI0);

/* single-element search */
wchar_t *wcschr(const wchar_t *, wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcschr", C2GO_GOABI0);
wchar_t *wcsrchr(const wchar_t *, wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsrchr", C2GO_GOABI0);

/* substring search */
wchar_t *wcsstr(const wchar_t *__restrict, const wchar_t *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsstr", C2GO_GOABI0);
wchar_t *wcswcs(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcswcs", C2GO_GOABI0);

/* span / break */
size_t   wcsspn(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsspn", C2GO_GOABI0);
size_t   wcscspn(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscspn", C2GO_GOABI0);
wchar_t *wcspbrk(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcspbrk", C2GO_GOABI0);

/* tokenise (3-arg reentrant) */
wchar_t *wcstok(wchar_t *__restrict, const wchar_t *__restrict, wchar_t **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstok", C2GO_GOABI0);

/* duplicate */
wchar_t *wcsdup(const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsdup", C2GO_GOABI0);

/* wide memory (wchar_t-count, not byte-count) */
wchar_t *wmemchr(const wchar_t *, wchar_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wmemchr", C2GO_GOABI0);
int      wmemcmp(const wchar_t *, const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wmemcmp", C2GO_GOABI0);
wchar_t *wmemcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wmemcpy", C2GO_GOABI0);
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wmemmove", C2GO_GOABI0);
wchar_t *wmemset(wchar_t *, wchar_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wmemset", C2GO_GOABI0);

/* wide numeric parsing (source/stdio.c, ported from musl src/stdlib/wcstol.c +
 * wcstod.c — the wide string is scanned through the shared __intscan/__floatscan
 * machinery). Each c2go_extern definition needs its matching linkname here. The
 * greatest-width twins wcstoimax/wcstoumax live in <inttypes.h> (intmax_t). */

/* wide string -> floating point */
float       wcstof(const wchar_t *__restrict, wchar_t **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstof", C2GO_GOABI0);
double      wcstod(const wchar_t *__restrict, wchar_t **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstod", C2GO_GOABI0);
long double wcstold(const wchar_t *__restrict, wchar_t **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstold", C2GO_GOABI0);

/* wide string -> integer */
long               wcstol(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstol", C2GO_GOABI0);
unsigned long      wcstoul(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstoul", C2GO_GOABI0);
long long          wcstoll(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstoll", C2GO_GOABI0);
unsigned long long wcstoull(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstoull", C2GO_GOABI0);

/* wide character FILE I/O (source/stdio.c, ported from musl src/stdio's
 * fwide/fputwc/fgetwc/... with the per-thread locale switching removed — c2go is
 * UTF-8-only). The FILE `mode` field holds the byte/wide orientation. Each
 * c2go_extern definition needs its matching linkname here. Managed FILE
 * replacement mode hides the root family; <c2go/mlib/wchar.h> supplies the
 * matching managed wrappers. Routing an mlib FILE into root libc remains
 * ABI-unsafe. */

#ifndef C2GO_MLIB_FILE_REPLACEMENT

int    fwide(FILE *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fwide", C2GO_GOABI0);

wint_t fputwc(wchar_t, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fputwc", C2GO_GOABI0);
wint_t putwc(wchar_t, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.putwc", C2GO_GOABI0);
wint_t putwchar(wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.putwchar", C2GO_GOABI0);

wint_t fgetwc(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fgetwc", C2GO_GOABI0);
wint_t getwc(FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getwc", C2GO_GOABI0);
wint_t getwchar(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.getwchar", C2GO_GOABI0);

int      fputws(const wchar_t *__restrict, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fputws", C2GO_GOABI0);
wchar_t *fgetws(wchar_t *__restrict, int, FILE *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.fgetws", C2GO_GOABI0);

wint_t ungetwc(wint_t, FILE *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ungetwc", C2GO_GOABI0);

#endif

/* wide FORMATTED output (source/stdio.c, ported from musl's src/stdio's
 * vfwprintf/vswprintf + the swprintf/fwprintf/wprintf/vwprintf wrappers). The
 * shared printf state machine / union arg / pop_arg are reused from the narrow
 * printf_core (same TU); the per-thread locale switching is dropped (c2go is
 * UTF-8-only) and the non-reentrant FILE lock forces the *_unlocked wide-output
 * path. Each c2go_extern definition needs its matching linkname here. Managed
 * FILE replacement mode hides the root stream family; <c2go/mlib/wchar.h>
 * supplies managed vfwprintf/fwprintf/vwprintf/wprintf wrappers while the
 * FILE-independent swprintf/vswprintf functions remain shared. */

#ifndef C2GO_MLIB_FILE_REPLACEMENT
int vfwprintf(FILE *__restrict, const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vfwprintf", C2GO_GOABI0);
int fwprintf(FILE *__restrict, const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.fwprintf", C2GO_GOABI0);
int wprintf(const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.wprintf", C2GO_GOABI0);
int vwprintf(const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vwprintf", C2GO_GOABI0);
#endif
int swprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.swprintf", C2GO_GOABI0);
int vswprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vswprintf", C2GO_GOABI0);

/* wide FORMATTED input (source/stdio.c, ported from musl's src/stdio's
 * vfwscanf + the swscanf/fwscanf/wscanf/vwscanf/vswscanf wrappers). store_int /
 * the SIZE_* macros and the __intscan/__floatscan scan primitives are reused
 * from the narrow vfscanf (same TU); the numeric conversions are inlined
 * (rather than musl's fscanf reuse) so the non-reentrant FILE lock is never
 * re-taken. Each c2go_extern definition needs its matching linkname here.
 * Managed FILE replacement mode hides all six root entry points because even
 * the string-only pair needs mlib's `%m` allocation and pointer-publication
 * policy; <c2go/mlib/wchar.h> supplies their managed replacements. */

#ifndef C2GO_MLIB_FILE_REPLACEMENT
int vfwscanf(FILE *__restrict, const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vfwscanf", C2GO_GOABI0);
int fwscanf(FILE *__restrict, const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.fwscanf", C2GO_GOABI0);
int wscanf(const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.wscanf", C2GO_GOABI0);
int vwscanf(const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vwscanf", C2GO_GOABI0);
int swscanf(const wchar_t *__restrict, const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.swscanf", C2GO_GOABI0);
int vswscanf(const wchar_t *__restrict, const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vswscanf", C2GO_GOABI0);
#endif

/* wide locale-aware collation (source/locale.c). c2go has only the C locale, so
 * these collate by code point; the _l variants ignore their locale_t. */
int      wcscoll(const wchar_t *, const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscoll", C2GO_GOABI0);
int      wcscoll_l(const wchar_t *, const wchar_t *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcscoll_l", C2GO_GOABI0);
size_t   wcsxfrm(wchar_t *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsxfrm", C2GO_GOABI0);
size_t   wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, size_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsxfrm_l", C2GO_GOABI0);

/* wide strftime (#652, source/time.c): each specifier is formatted by the
 * narrow C-locale strftime and widened via mbstowcs, musl's wcsftime shape.
 * One locale, so wcsftime_l ignores its locale_t. */
struct tm;
size_t   wcsftime(wchar_t *__restrict, size_t, const wchar_t *__restrict, const struct tm *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsftime", C2GO_GOABI0);
size_t   wcsftime_l(wchar_t *__restrict, size_t, const wchar_t *__restrict, const struct tm *__restrict, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcsftime_l", C2GO_GOABI0);

/* terminal column width (musl ctype/wcwidth.c tables; impl source/wctype.c).
 * musl's Unicode width tables are the semantics — NOT the host's. */
int wcwidth(wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcwidth", C2GO_GOABI0);
int wcswidth(const wchar_t *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcswidth", C2GO_GOABI0);

#endif /* _WCHAR_H */
