/* locale.h — C/POSIX locale support.
 *
 * c2go-libc provides ONLY the C.UTF-8 locale: it ships no locale definition
 * files, does no gettext/message translation, and (per the multibyte port) is
 * UTF-8-only with a constant MB_CUR_MAX of 4. So setlocale/newlocale always
 * yield the C.UTF-8 locale and collation is by code point — exactly how musl
 * itself behaves when no locale files are installed. This is a faithful minimal
 * locale, not a stub: every category genuinely has one, well-defined setting.
 *
 * strcoll/strxfrm live in <string.h>, wcscoll/wcsxfrm in <wchar.h> (with their
 * _l variants); this header owns setlocale/localeconv + the locale_t object
 * family. Each definition is c2go_extern (source/locale.c); every declaration
 * carries a matching c2go_linkname (the CC-consistency rule). */
#ifndef _LOCALE_H
#define _LOCALE_H

#include <c2go.h>

#define __NEED_locale_t
#define __NEED_size_t
#include <bits/alltypes.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define LC_CTYPE    0
#define LC_NUMERIC  1
#define LC_TIME     2
#define LC_COLLATE  3
#define LC_MONETARY 4
#define LC_MESSAGES 5
#define LC_ALL      6

struct lconv {
	char *decimal_point;
	char *thousands_sep;
	char *grouping;

	char *int_curr_symbol;
	char *currency_symbol;
	char *mon_decimal_point;
	char *mon_thousands_sep;
	char *mon_grouping;
	char *positive_sign;
	char *negative_sign;
	char int_frac_digits;
	char frac_digits;
	char p_cs_precedes;
	char p_sep_by_space;
	char n_cs_precedes;
	char n_sep_by_space;
	char p_sign_posn;
	char n_sign_posn;
	char int_p_cs_precedes;
	char int_p_sep_by_space;
	char int_n_cs_precedes;
	char int_n_sep_by_space;
	char int_p_sign_posn;
	char int_n_sign_posn;
};

char *setlocale(int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.setlocale", C2GO_GOABI0);
struct lconv *localeconv(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.localeconv", C2GO_GOABI0);

#define LC_GLOBAL_LOCALE ((locale_t)-1)

#define LC_CTYPE_MASK    (1<<LC_CTYPE)
#define LC_NUMERIC_MASK  (1<<LC_NUMERIC)
#define LC_TIME_MASK     (1<<LC_TIME)
#define LC_COLLATE_MASK  (1<<LC_COLLATE)
#define LC_MONETARY_MASK (1<<LC_MONETARY)
#define LC_MESSAGES_MASK (1<<LC_MESSAGES)
#define LC_ALL_MASK      0x7fffffff

locale_t duplocale(locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.duplocale", C2GO_GOABI0);
void     freelocale(locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.freelocale", C2GO_GOABI0);
locale_t newlocale(int, const char *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.newlocale", C2GO_GOABI0);
locale_t uselocale(locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.uselocale", C2GO_GOABI0);

#endif /* _LOCALE_H */
