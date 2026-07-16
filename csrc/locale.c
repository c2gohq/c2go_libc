/* locale.c — the C.UTF-8 locale (c2go-libc's only locale).
 *
 * c2go-libc ships no locale definition files and does no message translation,
 * so every category has exactly one setting (C.UTF-8): setlocale/newlocale
 * always yield it, localeconv returns the fixed C/POSIX lconv, and collation is
 * by code point. strcoll/strxfrm/wcscoll/wcsxfrm are ported verbatim from musl's
 * src/locale (their C-locale bodies ARE code-point compare/copy). This is the
 * faithful behavior of a musl with no locale files installed — see the header. */
#include <locale.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>   /* strtod/strtof/strtold for the strto*_l forwarders */
#include <limits.h>
#include <langinfo.h>  /* nl_item + the item codes (#648) */
#include <monetary.h>  /* strfmon/strfmon_l (#652) */
#include <stdio.h>     /* snprintf in vstrfmon_l */
#include <stdarg.h>    /* strfmon is variadic */
#include <ctype.h>     /* isdigit in vstrfmon_l's width parse */
#include <errno.h>     /* E2BIG */
#include <c2go.h>

/* The single locale object; every locale_t points here (opaque to callers,
 * never allocated, so freelocale is a no-op). */
struct __locale_struct { int __c2go_c_locale; };
static struct __locale_struct __c_locale;

/* setlocale: only C.UTF-8 exists behaviourally, but the NAME is bookkept per
 * category like musl's (#674):
 *   - setting an alias ("", "C.UTF-8" -> "C.UTF-8"; "C", "POSIX" -> "C")
 *     records and echoes that name — Lua's suite asserts
 *     setlocale(LC_ALL,"C") then setlocale(NULL) == "C";
 *   - any OTHER name is honestly rejected with NULL. musl accepts unknown
 *     names because MUSL_LOCPATH may load them dynamically — this libc ships
 *     no locale files ever, so accepting "pt_BR" while behaving as C would
 *     lie to callers that PROBE via the return value (Lua strings/literals
 *     take the glibc reject-unknown path). LC_ALL composite ";" strings are
 *     rejected too (nothing here ever produces a mixed state to restore).
 * The behaviour of every category stays C.UTF-8 regardless of the recorded
 * name (C and C.UTF-8 differ only in charset, and our ctype/mb layer is
 * unconditionally UTF-8 — the header documents this). */
static char __lc_c[] = "C", __lc_c_utf8[] = "C.UTF-8";
static char *__lc_name[LC_ALL] = {
	__lc_c_utf8, __lc_c_utf8, __lc_c_utf8,
	__lc_c_utf8, __lc_c_utf8, __lc_c_utf8,
};
c2go_extern
char *setlocale(int cat, const char *name)
{
	if ((unsigned)cat > LC_ALL) return 0;
	if (name) {
		char *rec;
		if (!*name || !strcmp(name, "C.UTF-8")) rec = __lc_c_utf8;
		else if (!strcmp(name, "C") || !strcmp(name, "POSIX")) rec = __lc_c;
		else return 0;
		if (cat == LC_ALL)
			for (int i = 0; i < LC_ALL; i++) __lc_name[i] = rec;
		else
			__lc_name[cat] = rec;
	}
	if (cat != LC_ALL) return __lc_name[cat];
	/* LC_ALL query: all six categories agree unless someone mixed them via
	 * per-category sets; echo musl's ";"-joined composite in that case. */
	for (int i = 1; i < LC_ALL; i++)
		if (__lc_name[i] != __lc_name[0])
			goto mixed;
	return __lc_name[0];
mixed:;
	static char comp[6 * (sizeof "C.UTF-8")];
	char *s = comp;
	for (int i = 0; i < LC_ALL; i++) {
		size_t l = strlen(__lc_name[i]);
		memcpy(s, __lc_name[i], l);
		s[l] = ';';
		s += l + 1;
	}
	s[-1] = 0;
	return comp;
}

/* The C/POSIX lconv (musl src/locale/localeconv.c, verbatim). */
static const struct lconv posix_lconv = {
	.decimal_point = ".",
	.thousands_sep = "",
	.grouping = "",
	.int_curr_symbol = "",
	.currency_symbol = "",
	.mon_decimal_point = "",
	.mon_thousands_sep = "",
	.mon_grouping = "",
	.positive_sign = "",
	.negative_sign = "",
	.int_frac_digits = CHAR_MAX,
	.frac_digits = CHAR_MAX,
	.p_cs_precedes = CHAR_MAX,
	.p_sep_by_space = CHAR_MAX,
	.n_cs_precedes = CHAR_MAX,
	.n_sep_by_space = CHAR_MAX,
	.p_sign_posn = CHAR_MAX,
	.n_sign_posn = CHAR_MAX,
	.int_p_cs_precedes = CHAR_MAX,
	.int_p_sep_by_space = CHAR_MAX,
	.int_n_cs_precedes = CHAR_MAX,
	.int_n_sep_by_space = CHAR_MAX,
	.int_p_sign_posn = CHAR_MAX,
	.int_n_sign_posn = CHAR_MAX,
};

c2go_extern
struct lconv *localeconv(void)
{
	return (struct lconv *)&posix_lconv;
}

/* (collation / transform — strcoll/strxfrm/wcscoll/wcsxfrm and their _l
 * variants — is musl verbatim and lives in the musl fork, src/locale/
 * {strcoll,strxfrm,wcscoll,wcsxfrm}.c. C locale == code-point order.) */

/* ── float string->number, locale variants (musl src/locale/strtod_l.c). musl
 * provides ONLY the float strto*_l (the decimal point is the only locale-
 * sensitive input, and musl keeps it "." always), so these forward. There are
 * no integer strtol_l / no wide wcsto*_l in musl. ──────────────────────────── */

c2go_extern
float strtof_l(const char *restrict s, char **restrict p, locale_t l)
{
	(void)l;
	return strtof(s, p);
}

c2go_extern
double strtod_l(const char *restrict s, char **restrict p, locale_t l)
{
	(void)l;
	return strtod(s, p);
}

c2go_extern
long double strtold_l(const char *restrict s, char **restrict p, locale_t l)
{
	(void)l;
	return strtold(s, p);
}

/* ── locale_t object family: all requests yield the single C.UTF-8 locale ─── */

c2go_extern
locale_t newlocale(int mask, const char *name, locale_t base)
{
	(void)mask;
	(void)base;
	/* Mirror setlocale's honest-reject (#674): only the C.UTF-8 aliases
	 * name a locale this libc can actually provide. */
	if (!name || !*name || !strcmp(name, "C") || !strcmp(name, "POSIX") ||
	    !strcmp(name, "C.UTF-8"))
		return &__c_locale;
	return 0;
}

c2go_extern
void freelocale(locale_t loc)
{
	(void)loc; /* the C locale is static, never allocated */
}

c2go_extern
locale_t duplocale(locale_t loc)
{
	(void)loc;
	return &__c_locale;
}

c2go_extern
locale_t uselocale(locale_t loc)
{
	/* Switching is unobservable (one locale), so this is a faithful no-op: it
	 * reports the thread's locale as the global (C.UTF-8) one either way, which
	 * keeps the save/restore idiom `old=uselocale(x); …; uselocale(old)` sound. */
	(void)loc;
	return LC_GLOBAL_LOCALE;
}

/* ── nl_langinfo (#648, musl src/locale/nl_langinfo.c shape) ────────────────
 * The C locale's fixed answers — the only locale setlocale accepts — with
 * CODESET = "UTF-8" (this libc is UTF-8 throughout). Item encoding is
 * (category << 16 | index), matching <langinfo.h>/musl. The per-category
 * string tables are NUL-joined runs indexed by walking, musl-style. */

static const char c_time_table[] =
	"Sun\0" "Mon\0" "Tue\0" "Wed\0" "Thu\0" "Fri\0" "Sat\0"
	"Sunday\0" "Monday\0" "Tuesday\0" "Wednesday\0"
	"Thursday\0" "Friday\0" "Saturday\0"
	"Jan\0" "Feb\0" "Mar\0" "Apr\0" "May\0" "Jun\0"
	"Jul\0" "Aug\0" "Sep\0" "Oct\0" "Nov\0" "Dec\0"
	"January\0"   "February\0" "March\0"    "April\0"
	"May\0"       "June\0"     "July\0"     "August\0"
	"September\0" "October\0"  "November\0" "December\0"
	"AM\0" "PM\0"
	"%a %b %e %T %Y\0"
	"%m/%d/%y\0"
	"%H:%M:%S\0"
	"%I:%M:%S %p\0"
	"\0"                /* ERA (empty in musl's C locale) */
	"\0"                /* unused slot 0x2D (musl keeps the gap) */
	"%m/%d/%y\0"        /* ERA_D_FMT   — musl langinfo.c values (#657): the */
	"0123456789\0"      /* ALT_DIGITS    ERA_* items fall back to the plain */
	"%a %b %e %T %Y\0"  /* ERA_D_T_FMT   formats and ALT_DIGITS is ASCII    */
	"%H:%M:%S";          /* ERA_T_FMT     digits, none of them empty.        */

static const char c_messages_table[] = "^[yY]\0" "^[nN]\0" "yes\0" "no";  /* + legacy YESSTR/NOSTR (musl) */

c2go_extern char *nl_langinfo(nl_item item)
{
	int cat = item >> 16;
	unsigned idx = item & 0xffff;
	const char *str;

	if (item == CODESET)
		return (char *)"UTF-8";

	switch (cat) {
	case 1: /* LC_NUMERIC: RADIXCHAR / THOUSEP */
		if (idx > 1) return (char *)"";
		return (char *)(idx ? "" : ".");
	case 2: /* LC_TIME */
		if (idx > 0x31) return (char *)"";
		str = c_time_table;
		break;
	case 4: /* LC_MONETARY: CRNCYSTR */
		return (char *)"";
	case 5: /* LC_MESSAGES: YESEXPR / NOEXPR / YESSTR / NOSTR */
		if (idx > 3) return (char *)"";
		str = c_messages_table;
		break;
	default:
		return (char *)"";
	}
	for (; idx; idx--, str++)
		for (; *str; str++);
	return (char *)str;
}

/* Only the C locale exists here, so the _l variant ignores loc. */
c2go_extern char *nl_langinfo_l(nl_item item, locale_t loc)
{
	(void)loc;
	return nl_langinfo(item);
}

/* ── strfmon (#652, musl src/locale/strfmon.c verbatim minus locale_impl) ───
 * The C locale defines no currency symbol, grouping, or sign layout, so the
 * parsed flags (fill/^/(/!/-) have nothing to apply to and the conversion is
 * plain "%*.*f" — that IS musl's behavior, not a shortcut. */

static ssize_t vstrfmon_l(char *s, size_t n, locale_t loc, const char *fmt, va_list ap)
{
	size_t l;
	double x;
	int fill, nogrp, negpar, nosym, left, intl;
	int lp, rp, w, fw;
	char *s0=s;
	(void)loc;
	for (; n && *fmt; ) {
		if (*fmt != '%') {
		literal:
			*s++ = *fmt++;
			n--;
			continue;
		}
		fmt++;
		if (*fmt == '%') goto literal;

		fill = ' ';
		nogrp = 0;
		negpar = 0;
		nosym = 0;
		left = 0;
		for (; ; fmt++) {
			switch (*fmt) {
			case '=':
				fill = *++fmt;
				continue;
			case '^':
				nogrp = 1;
				continue;
			case '(':
				negpar = 1;
			case '+':
				continue;
			case '!':
				nosym = 1;
				continue;
			case '-':
				left = 1;
				continue;
			}
			break;
		}
		(void)fill; (void)nogrp; (void)negpar; (void)nosym;

		for (fw=0; isdigit(*fmt); fmt++)
			fw = 10*fw + (*fmt-'0');
		lp = 0;
		rp = 2;
		if (*fmt=='#') for (lp=0, fmt++; isdigit(*fmt); fmt++)
			lp = 10*lp + (*fmt-'0');
		if (*fmt=='.') for (rp=0, fmt++; isdigit(*fmt); fmt++)
			rp = 10*rp + (*fmt-'0');

		intl = *fmt++ == 'i';
		(void)intl;

		w = lp + 1 + rp;
		if (!left && fw>w) w = fw;

		x = va_arg(ap, double);
		l = snprintf(s, n, "%*.*f", w, rp, x);
		if (l >= n) {
			errno = E2BIG;
			return -1;
		}
		s += l;
		n -= l;
	}
	return s-s0;
}

c2go_extern
ssize_t strfmon_l(char *restrict s, size_t n, locale_t loc, const char *restrict fmt, ...)
{
	va_list ap;
	ssize_t ret;

	va_start(ap, fmt);
	ret = vstrfmon_l(s, n, loc, fmt, ap);
	va_end(ap);

	return ret;
}

c2go_extern
ssize_t strfmon(char *restrict s, size_t n, const char *restrict fmt, ...)
{
	va_list ap;
	ssize_t ret;

	va_start(ap, fmt);
	ret = vstrfmon_l(s, n, 0, fmt, ap);
	va_end(ap);

	return ret;
}
