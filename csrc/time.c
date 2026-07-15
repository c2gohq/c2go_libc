/* time.c — <time.h>/<sys/time.h> clock reads + calendar math.
 *
 * The "now" primitive is a Go bridge (time.go): __c2go_clock_gettime fills
 * (sec, nsec) for a c2go-uniform clock id, __c2go_nanosleep parks the goroutine.
 * The time_t/timespec/timeval shaping and the UTC calendar conversions (ported
 * from musl) are C here. There is NO C-side timezone database: the LOCAL zone
 * offset / DST flag / abbreviation come from Go's time package (time.go's
 * __c2go_local_offset / __c2go_mktime / __c2go_tzset, i.e. time.Local honouring
 * /etc/localtime or $TZ). gmtime stays pure-C UTC; localtime/mktime apply the
 * Go-supplied offset; timegm is pure-C UTC (NOT local). */
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>   /* snprintf for asctime_r */
#include <stdlib.h>  /* strtoul in strptime width fields */
#include <ctype.h>   /* isdigit/isspace in strptime */
#include <string.h>  /* strlen/strncmp in strptime %Z */
#include <strings.h> /* strncasecmp in strptime symbolic/AM-PM matching */
#include <stddef.h>  /* offsetof in strptime */
#include <unistd.h>  /* sleep/usleep decls (goabi0 CC consistency) */
#include <wchar.h>   /* wcsftime decl + wmemcpy (#652) */
#include <c2go.h>

/* ── Go clock bridge (time.go) ── */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_clock_gettime", C2GO_GOABI0)
int __c2go_clock_gettime(int id, long long *sec, long long *nsec);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_nanosleep", C2GO_GOABI0)
void __c2go_nanosleep(long long sec, long long nsec);

/* ── Go timezone bridge (time.go) ── */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_local_offset", C2GO_GOABI0)
void __c2go_local_offset(long long sec, long long *off, int *isdst, const char **zone);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_mktime", C2GO_GOABI0)
long long __c2go_mktime(int year, int mon, int mday, int hour, int min, int sec,
                        long long *off, int *isdst, const char **zone);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_tzset", C2GO_GOABI0)
void __c2go_tzset(int reload, const char **std, const char **dst, long long *stdwest, int *hasdst);

/* POSIX: localtime/mktime behave as if tzset() had been called, so tzname etc.
 * are valid after them. __ensure_tz runs the SNAPSHOT (reload=0, never touching
 * time.Local) once on first use — so a program reading tzname after localtime()
 * without an explicit tzset() sees the real zone, not the initial "UTC"
 * placeholder. Defined near tzset() below. */
static void __ensure_tz(void);

/* ── clock reads ── */

c2go_extern int clock_gettime(clockid_t id, struct timespec *ts) {
    long long s, n;
    int r = __c2go_clock_gettime(id, &s, &n);
    if (r != 0) { errno = r; return -1; }
    if (ts) { ts->tv_sec = s; ts->tv_nsec = n; }
    return 0;
}

c2go_extern time_t time(time_t *t) {
    long long s, n;
    __c2go_clock_gettime(CLOCK_REALTIME, &s, &n);
    if (t) *t = (time_t)s;
    return (time_t)s;
}

c2go_extern int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz; /* the obsolete struct timezone is ignored */
    long long s, n;
    __c2go_clock_gettime(CLOCK_REALTIME, &s, &n);
    if (tv) { tv->tv_sec = s; tv->tv_usec = n / 1000; }
    return 0;
}

c2go_extern int clock_getres(clockid_t id, struct timespec *ts) {
    /* Validate id against the same set clock_gettime accepts (single source
     * of truth in time.go); musl lets the kernel reject bad ids (#657). */
    long long s, n;
    int r = __c2go_clock_gettime(id, &s, &n);
    if (r != 0) { errno = r; return -1; }
    if (ts) { ts->tv_sec = 0; ts->tv_nsec = 1; } /* nominal 1ns resolution */
    return 0;
}

c2go_extern int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999) {
        errno = EINVAL;
        return -1;
    }
    __c2go_nanosleep(req->tv_sec, req->tv_nsec);
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; } /* no early wakeup */
    return 0;
}

/* sleep/usleep (musl unistd, faithful): thin wrappers over nanosleep. Because our
 * nanosleep never wakes early (rem is always zeroed, return is 0 for a valid
 * request), sleep always returns 0 and usleep returns nanosleep's status. */

c2go_extern unsigned sleep(unsigned seconds) {
    struct timespec tv = { .tv_sec = seconds, .tv_nsec = 0 };
    if (nanosleep(&tv, &tv))
        return tv.tv_sec; /* interrupted: seconds remaining */
    return 0;
}

c2go_extern int usleep(unsigned useconds) {
    struct timespec tv = {
        .tv_sec = useconds / 1000000,
        .tv_nsec = (useconds % 1000000) * 1000,
    };
    return nanosleep(&tv, &tv);
}

c2go_extern clock_t clock(void) {
    /* No process-CPU clock under the Go runtime; approximate with monotonic
     * process-elapsed time. CLOCKS_PER_SEC == 1000000, so return microseconds. */
    long long s, n;
    __c2go_clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s, &n);
    return (clock_t)(s * 1000000 + n / 1000);
}

c2go_extern int timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    return clock_gettime(CLOCK_REALTIME, ts) == 0 ? base : 0;
}

c2go_extern double difftime(time_t a, time_t b) {
    return (double)(a - b);
}

/* ── UTC calendar conversions (musl src/time, verbatim; no timezone DB) ──────
 * __secs_to_tm / __tm_to_secs are the reference algorithms; the public
 * gmtime/localtime/mktime wrap them. localtime == gmtime here (UTC). */

/* 2000-03-01 (mod 400 year, immediately after feb29). */
#define LEAPOCH       (946684800LL + 86400 * (31 + 29))
#define DAYS_PER_400Y (365 * 400 + 97)
#define DAYS_PER_100Y (365 * 100 + 24)
#define DAYS_PER_4Y   (365 * 4 + 1)

static int __secs_to_tm(long long t, struct tm *tm) {
    long long days, secs, years;
    int remdays, remsecs, remyears;
    int qc_cycles, c_cycles, q_cycles;
    int months;
    int wday, yday, leap;
    static const char days_in_month[] = {31,30,31,30,31,31,30,31,30,31,31,29};

    if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL)
        return -1;

    secs = t - LEAPOCH;
    days = secs / 86400;
    remsecs = secs % 86400;
    if (remsecs < 0) { remsecs += 86400; days--; }

    wday = (3 + days) % 7;
    if (wday < 0) wday += 7;

    qc_cycles = days / DAYS_PER_400Y;
    remdays = days % DAYS_PER_400Y;
    if (remdays < 0) { remdays += DAYS_PER_400Y; qc_cycles--; }

    c_cycles = remdays / DAYS_PER_100Y;
    if (c_cycles == 4) c_cycles--;
    remdays -= c_cycles * DAYS_PER_100Y;

    q_cycles = remdays / DAYS_PER_4Y;
    if (q_cycles == 25) q_cycles--;
    remdays -= q_cycles * DAYS_PER_4Y;

    remyears = remdays / 365;
    if (remyears == 4) remyears--;
    remdays -= remyears * 365;

    leap = !remyears && (q_cycles || !c_cycles);
    yday = remdays + 31 + 28 + leap;
    if (yday >= 365 + leap) yday -= 365 + leap;

    years = remyears + 4 * q_cycles + 100 * c_cycles + 400 * qc_cycles;

    for (months = 0; days_in_month[months] <= remdays; months++)
        remdays -= days_in_month[months];

    if (months >= 10) { months -= 12; years++; }

    if (years + 100 > INT_MAX || years + 100 < INT_MIN)
        return -1;

    tm->tm_year = years + 100;
    tm->tm_mon = months + 2;
    tm->tm_mday = remdays + 1;
    tm->tm_wday = wday;
    tm->tm_yday = yday;
    tm->tm_hour = remsecs / 3600;
    tm->tm_min = remsecs / 60 % 60;
    tm->tm_sec = remsecs % 60;
    return 0;
}

static long long __year_to_secs(long long year, int *is_leap) {
    int dummy;
    if (!is_leap) is_leap = &dummy;
    if (year - 2ULL <= 136) {
        int y = year;
        int leaps = (y - 68) >> 2;
        if (!((y - 68) & 3)) { leaps--; *is_leap = 1; }
        else *is_leap = 0;
        return 31536000LL * (y - 70) + 86400LL * leaps;
    }
    int cycles, centuries, leaps, rem;
    cycles = (year - 100) / 400;
    rem = (year - 100) % 400;
    if (rem < 0) { cycles--; rem += 400; }
    if (!rem) { *is_leap = 1; centuries = 0; leaps = 0; }
    else {
        if (rem >= 200) { if (rem >= 300) centuries = 3, rem -= 300; else centuries = 2, rem -= 200; }
        else { if (rem >= 100) centuries = 1, rem -= 100; else centuries = 0; }
        if (!rem) { *is_leap = 0; leaps = 0; }
        else { leaps = rem / 4U; rem %= 4U; *is_leap = !rem; }
    }
    leaps += 97 * cycles + 24 * centuries - *is_leap;
    return (year - 100) * 31536000LL + leaps * 86400LL + 946684800LL + 86400LL;
}

static int __month_to_secs(int month, int is_leap) {
    static const int secs_through_month[] = {
        0, 31*86400, 59*86400, 90*86400, 120*86400, 151*86400,
        181*86400, 212*86400, 243*86400, 273*86400, 304*86400, 334*86400};
    int t = secs_through_month[month];
    if (is_leap && month >= 2) t += 86400;
    return t;
}

static long long __tm_to_secs(const struct tm *tm) {
    int is_leap;
    long long year = tm->tm_year;
    int month = tm->tm_mon;
    if (month >= 12 || month < 0) {
        int adj = month / 12;
        month %= 12;
        if (month < 0) { adj--; month += 12; }
        year += adj;
    }
    long long t = __year_to_secs(year, &is_leap);
    t += __month_to_secs(month, is_leap);
    t += 86400LL * (tm->tm_mday - 1);
    t += 3600LL * tm->tm_hour;
    t += 60LL * tm->tm_min;
    t += tm->tm_sec;
    return t;
}

c2go_extern struct tm *gmtime_r(const time_t *restrict t, struct tm *restrict tm) {
    if (__secs_to_tm(*t, tm) < 0) { errno = EOVERFLOW; return 0; }
    tm->tm_isdst = 0;
    tm->tm_gmtoff = 0;
    tm->tm_zone = "UTC";
    return tm;
}

c2go_extern struct tm *gmtime(const time_t *t) {
    static struct tm tm;
    return gmtime_r(t, &tm);
}

/* localtime applies the Go-supplied local zone offset to the instant, then runs
 * the same UTC calendar math on the shifted seconds. tm_gmtoff/tm_isdst/tm_zone
 * carry the zone in effect at that instant (DST-aware). */
c2go_extern struct tm *localtime_r(const time_t *restrict t, struct tm *restrict tm) {
    long long off; int isdst; const char *zone;
    __ensure_tz();
    __c2go_local_offset((long long)*t, &off, &isdst, &zone);
    if (__secs_to_tm((long long)*t + off, tm) < 0) { errno = EOVERFLOW; return 0; }
    tm->tm_isdst = isdst;
    tm->tm_gmtoff = off;
    tm->tm_zone = zone;
    return tm;
}

c2go_extern struct tm *localtime(const time_t *t) {
    static struct tm tm;
    return localtime_r(t, &tm);
}

/* mktime: the struct tm is LOCAL wall-clock time. time.Date (via the bridge)
 * resolves the offset — including the DST fold and out-of-range field
 * normalisation — and returns the Unix instant; we refill the broken-down fields
 * from that instant in local time. tm_year is years since 1900 and tm_mon is
 * 0..11, so add 1900 / 1 for the Gregorian year / 1-based month Go expects. */
c2go_extern time_t mktime(struct tm *tm) {
    long long off; int isdst; const char *zone;
    __ensure_tz();
    long long u = __c2go_mktime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                                tm->tm_hour, tm->tm_min, tm->tm_sec, &off, &isdst, &zone);
    if (__secs_to_tm(u + off, tm) < 0) { errno = EOVERFLOW; return -1; }
    tm->tm_isdst = isdst;
    tm->tm_gmtoff = off;
    tm->tm_zone = zone;
    return (time_t)u;
}

/* timegm is the UTC inverse of gmtime: it ignores the local zone entirely —
 * interpret the struct tm as UTC and return the epoch seconds, normalising the
 * broken-down fields via a round-trip. */
c2go_extern time_t timegm(struct tm *tm) {
    long long t = __tm_to_secs(tm);
    if (__secs_to_tm(t, tm) < 0) { errno = EOVERFLOW; return -1; }
    tm->tm_isdst = 0;
    tm->tm_gmtoff = 0;
    tm->tm_zone = "UTC";
    return (time_t)t;
}

static const char __wday_name[7][4] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char __mon_name[12][4] = {"Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec"};

c2go_extern char *asctime_r(const struct tm *restrict tm, char *restrict buf) {
    /* "Www Mmm dd hh:mm:ss yyyy\n" + NUL == 26 bytes. Guard the name indices. */
    if (tm->tm_wday < 0 || tm->tm_wday > 6 || tm->tm_mon < 0 || tm->tm_mon > 11)
        return 0;
    if (snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
                 __wday_name[tm->tm_wday], __mon_name[tm->tm_mon],
                 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                 1900 + tm->tm_year) >= 26) {
        /* musl a_crash()es here (its sprintf would smash the 26-byte buffer);
         * our snprintf cannot smash, so the softer honest failure is
         * EOVERFLOW + NULL (#657, documented deviation). */
        errno = EOVERFLOW;
        return 0;
    }
    return buf;
}

c2go_extern char *asctime(const struct tm *tm) {
    static char buf[26];
    return asctime_r(tm, buf);
}

c2go_extern char *ctime_r(const time_t *t, char *buf) {
    struct tm tm;
    if (!localtime_r(t, &tm)) return 0;
    return asctime_r(&tm, buf);
}

c2go_extern char *ctime(const time_t *t) {
    struct tm *tm = localtime(t);
    return tm ? asctime(tm) : 0;
}

/* ── strftime (C locale; UTC timezone) ──────────────────────────────────────
 * Self-contained: the common conversion specifiers formatted for the "C" locale.
 * %U/%W are Sunday/Monday-first week numbers; %z/%Z are UTC. Returns the byte
 * count (excluding NUL), or 0 if the result would not fit in n bytes. */

static const char *const __wday_full[7] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char *const __mon_full[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December"};

/* musl src/time/strftime.c is_leap/week_num, verbatim (#657): ISO 8601 week
 * number for %V and the week-based-year adjustment for %G/%g. */
static int __sft_is_leap(int y)
{
    /* Avoid overflow */
    if (y > INT_MAX-1900) y -= 2000;
    y += 1900;
    return !(y%4) && ((y%100) || !(y%400));
}

static int __sft_week_num(const struct tm *tm)
{
    int val = (tm->tm_yday + 7U - (tm->tm_wday+6U)%7) / 7;
    /* If 1 Jan is just 1-3 days past Monday, the previous week
     * is also in this year. */
    if ((tm->tm_wday + 371U - tm->tm_yday - 2) % 7 <= 2)
        val++;
    if (!val) {
        val = 52;
        /* If 31 December of prev year a Thursday, or Friday of a
         * leap year, then the prev year has 53 weeks. */
        int dec31 = (tm->tm_wday + 7U - tm->tm_yday - 1) % 7;
        if (dec31 == 4 || (dec31 == 5 && __sft_is_leap(tm->tm_year%400-1)))
            val++;
    } else if (val == 53) {
        /* If 1 January is not a Thursday, and not a Wednesday of a
         * leap year, then this year has only 52 weeks. */
        int jan1 = (tm->tm_wday + 371U - tm->tm_yday) % 7;
        if (jan1 != 4 && (jan1 != 3 || !__sft_is_leap(tm->tm_year)))
            val = 1;
    }
    return val;
}

c2go_extern size_t strftime(char *restrict s, size_t n, const char *restrict f,
                            const struct tm *restrict tm) {
    size_t i = 0;
    char tmp[64];
    int wd = tm->tm_wday & 7, mo = tm->tm_mon;
    if (wd > 6) wd = 0;
    if (mo < 0 || mo > 11) mo = 0;

#define PUTC(c)   do { if (i >= n) return 0; s[i++] = (char)(c); } while (0)
#define PUTS(str) do { const char *_p = (str); while (*_p) { if (i >= n) return 0; s[i++] = *_p++; } } while (0)
#define PUTF(...) do { snprintf(tmp, sizeof tmp, __VA_ARGS__); PUTS(tmp); } while (0)

    for (; *f; f++) {
        if (*f != '%') { PUTC(*f); continue; }
        f++;
        if (*f == 'E' || *f == 'O') f++; /* ignore locale-alt modifiers */
        switch (*f) {
        case 'a': PUTS(__wday_name[wd]); break;
        case 'A': PUTS(__wday_full[wd]); break;
        case 'h':
        case 'b': PUTS(__mon_name[mo]); break;
        case 'B': PUTS(__mon_full[mo]); break;
        case 'c': PUTF("%.3s %.3s%3d %.2d:%.2d:%.2d %d",
                       __wday_name[wd], __mon_name[mo], tm->tm_mday,
                       tm->tm_hour, tm->tm_min, tm->tm_sec, 1900 + tm->tm_year); break;
        case 'C': PUTF("%02d", (1900 + tm->tm_year) / 100); break;
        case 'd': PUTF("%02d", tm->tm_mday); break;
        case 'D': PUTF("%02d/%02d/%02d", mo + 1, tm->tm_mday, (1900 + tm->tm_year) % 100); break;
        case 'e': PUTF("%2d", tm->tm_mday); break;
        case 'F': PUTF("%d-%02d-%02d", 1900 + tm->tm_year, mo + 1, tm->tm_mday); break;
        case 'H': PUTF("%02d", tm->tm_hour); break;
        case 'I': PUTF("%02d", (tm->tm_hour % 12) ? tm->tm_hour % 12 : 12); break;
        case 'j': PUTF("%03d", tm->tm_yday + 1); break;
        case 'm': PUTF("%02d", mo + 1); break;
        case 'M': PUTF("%02d", tm->tm_min); break;
        case 'n': PUTC('\n'); break;
        case 'p': PUTS(tm->tm_hour < 12 ? "AM" : "PM"); break;
        case 'r': PUTF("%02d:%02d:%02d %s", (tm->tm_hour % 12) ? tm->tm_hour % 12 : 12,
                       tm->tm_min, tm->tm_sec, tm->tm_hour < 12 ? "AM" : "PM"); break;
        case 'R': PUTF("%02d:%02d", tm->tm_hour, tm->tm_min); break;
        case 'S': PUTF("%02d", tm->tm_sec); break;
        case 't': PUTC('\t'); break;
        case 'T':
        case 'X': PUTF("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec); break;
        case 'u': PUTF("%d", wd == 0 ? 7 : wd); break;
        case 'U': PUTF("%02d", (tm->tm_yday + 7 - wd) / 7); break;
        case 'V': PUTF("%02d", __sft_week_num(tm)); break;
        case 'g':
        case 'G': { /* ISO week-based year (musl): shift ±1 at year edges */
            long long v = tm->tm_year + 1900LL;
            if (tm->tm_yday < 3 && __sft_week_num(tm) != 1) v--;
            else if (tm->tm_yday > 360 && __sft_week_num(tm) == 1) v++;
            if (*f == 'g') PUTF("%02d", (int)(v % 100));
            else PUTF("%04lld", v);
            break;
        }
        case 'w': PUTF("%d", wd); break;
        case 'W': PUTF("%02d", (tm->tm_yday + 7 - (wd ? wd - 1 : 6)) / 7); break;
        case 'x': PUTF("%02d/%02d/%02d", mo + 1, tm->tm_mday, (1900 + tm->tm_year) % 100); break;
        case 'y': PUTF("%02d", (1900 + tm->tm_year) % 100); break;
        case 'Y': PUTF("%d", 1900 + tm->tm_year); break;
        case 'z': {
            long g = tm->tm_gmtoff;
            long a = g < 0 ? -g : g;
            PUTF("%c%02ld%02ld", g < 0 ? '-' : '+', a / 3600, (a % 3600) / 60);
            break;
        }
        case 'Z': PUTS(tm->tm_zone ? tm->tm_zone : ""); break;
        case '%': PUTC('%'); break;
        /* musl (#657): an unknown conversion (or a trailing '%') makes the
         * whole call FAIL (return 0) — it does not echo the specifier. */
        case '\0': return 0; /* trailing '%' */
        default:  return 0;   /* unknown conversion */
        }
    }
    if (i >= n) return 0;
    s[i] = 0;
    return i;
#undef PUTC
#undef PUTS
#undef PUTF
}

/* musl's __strftime_l localizes month/day names via LC_TIME message catalogs;
 * c2go-libc ships none, so this forwards to the C-locale strftime above. */
c2go_extern size_t strftime_l(char *restrict s, size_t n, const char *restrict f,
                              const struct tm *restrict tm, locale_t loc) {
    (void)loc;
    return strftime(s, n, f, tm);
}

/* ── wcsftime (#652, musl src/time/wcsftime.c shape) ─────────────────────────
 * musl formats each conversion via the narrow per-specifier core and widens
 * the result with mbstowcs; literal wide chars copy through directly. Here the
 * narrow core is the self-contained C-locale strftime above, fed one "%[EO]X"
 * at a time (it skips E/O itself; a lone specifier always fits buf, and on the
 * empty case — e.g. %Z with no zone — it NUL-terminates and mbstowcs yields
 * 0). musl's glibc pad/width extensions (%-d, %04Y, %+F) are not in the
 * narrow core, so they are not accepted here either. Like musl, a trailing
 * '%' or an overflowing result returns 0 with the buffer NUL-terminated. */
c2go_extern size_t wcsftime(wchar_t *restrict s, size_t n, const wchar_t *restrict f,
                            const struct tm *restrict tm) {
    size_t l, k;
    int ci;
    char cf[4], buf[100];
    wchar_t wbuf[100];
    for (l = 0; l < n; f++) {
        if (!*f) {
            s[l] = 0;
            return l;
        }
        if (*f != '%') {
            s[l++] = *f;
            continue;
        }
        f++;
        cf[0] = '%';
        ci = 1;
        if (*f == 'E' || *f == 'O') cf[ci++] = (char)*f++;
        if (!*f || (unsigned long)*f > 0x7e) break; /* trailing '%' / non-ASCII spec */
        cf[ci] = (char)*f;
        cf[ci+1] = 0;
        if (!strftime(buf, sizeof buf, cf, tm) && *f != 'Z')
            break; /* unknown conversion: fail like musl (%Z alone may be empty) */
        k = mbstowcs(wbuf, buf, sizeof wbuf / sizeof *wbuf);
        if (k == (size_t)-1) return 0;
        if (k >= n - l) k = n - l;
        wmemcpy(s + l, wbuf, k);
        l += k;
    }
    if (n) {
        if (l == n) l = n - 1;
        s[l] = 0;
    }
    return 0;
}

/* One locale, so wcsftime_l ignores its locale_t (as strftime_l above). */
c2go_extern size_t wcsftime_l(wchar_t *restrict s, size_t n, const wchar_t *restrict f,
                              const struct tm *restrict tm, locale_t loc) {
    (void)loc;
    return wcsftime(s, n, f, tm);
}

/* ── tzset / tzname / timezone / daylight (POSIX globals) ────────────────────
 * Non-static → shared across TUs by symbol name (like stdout). localtime does
 * NOT depend on these (it queries the per-instant offset directly); they exist
 * only for programs that read tzname/timezone/daylight, and are (re)loaded from
 * the Go bridge on each tzset(). tzname points at interned, process-stable zone
 * names. `timezone` is seconds WEST of UTC for standard time (POSIX sign). */
char *tzname[2] = { (char *)"UTC", (char *)"UTC" };
long  timezone = 0;
int   daylight = 0;

/* __tz_refresh snapshots tzname/timezone/daylight from Go's time.Local. reload=1
 * (the public tzset) first re-reads $TZ into time.Local; reload=0 (the lazy
 * localtime/mktime ensure) only snapshots and never writes time.Local. */
static void __tz_refresh(int reload) {
    const char *std, *dst;
    long long west;
    int hasdst;
    __c2go_tzset(reload, &std, &dst, &west, &hasdst);
    /* The zone-name pointers are GO strings stored in C globals (non-zero-init,
     * so NOT #646-ceded GC roots): their liveness rests on time.go interning
     * the names process-stable (#661 anchor — verified; keep that invariant). */
    tzname[0] = (char *)std;
    tzname[1] = (char *)dst;
    timezone = (long)west;
    daylight = hasdst;
}

/* tzset re-reads $TZ into Go's time.Local (the single source of truth shared with
 * Go's own time package) and snapshots the resolved zone into these globals. */
c2go_extern void tzset(void) {
    __tz_refresh(1);
}

/* One-time lazy snapshot for localtime/mktime (declared up top). Refreshing on
 * FIRST use only fixes the pre-tzset "UTC" placeholder; a later zone change still
 * needs an explicit tzset() to refresh the globals (POSIX), which is why this is
 * gated rather than run per call. The check-then-set race is benign — a redundant
 * snapshot is idempotent. */
static int __tz_ready = 0;
static void __ensure_tz(void) {
    if (!__tz_ready) {
        __tz_ready = 1;
        __tz_refresh(0);
    }
}

/* ── strptime (C locale; musl src/time/strptime.c) ──────────────────────────
 * Faithful port: nl_langinfo is replaced by the C-locale name tables above and
 * inlined D_T_FMT/D_FMT/T_FMT/T_FMT_AMPM formats; symbolic day/month matching is
 * done inline (full name first, then abbreviation, case-insensitive) instead of
 * musl's langinfo index walk. %Z matches the tzset() tzname[] pair. */

/* Match a full-or-abbrev name (case-insensitive, longest first) from parallel
 * tables of `n` entries; on match set *out to the index and advance *sp. */
static int __strp_name(const char **sp, const char *const *full,
                       const char (*abbr)[4], int n, int *out) {
    const char *s = *sp;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(full[i]);
        if (!strncasecmp(s, full[i], l)) { *out = i; *sp = s + l; return 1; }
        l = strlen(abbr[i]);
        if (!strncasecmp(s, abbr[i], l)) { *out = i; *sp = s + l; return 1; }
    }
    return 0;
}

c2go_extern char *strptime(const char *restrict s, const char *restrict f,
                           struct tm *restrict tm) {
    int i, w, neg, adj, min, range, *dest, dummy;
    size_t len;
    int want_century = 0, century = 0, relyear = 0;
    while (*f) {
        if (*f != '%') {
            if (isspace((unsigned char)*f)) for (; *s && isspace((unsigned char)*s); s++);
            else if (*s != *f) return 0;
            else s++;
            f++;
            continue;
        }
        f++;
        if (*f == '+') f++;
        if (isdigit((unsigned char)*f)) {
            char *new_f;
            w = strtoul(f, &new_f, 10);
            f = new_f;
        } else {
            w = -1;
        }
        adj = 0;
        switch (*f++) {
        case 'a': case 'A':
            if (!__strp_name(&s, __wday_full, __wday_name, 7, &tm->tm_wday)) return 0;
            break;
        case 'b': case 'B': case 'h':
            if (!__strp_name(&s, __mon_full, __mon_name, 12, &tm->tm_mon)) return 0;
            break;
        case 'c':
            s = strptime(s, "%a %b %e %H:%M:%S %Y", tm);
            if (!s) return 0;
            break;
        case 'C':
            dest = &century;
            if (w < 0) w = 2;
            want_century |= 2;
            goto numeric_digits;
        case 'd': case 'e':
            dest = &tm->tm_mday;
            min = 1;
            range = 31;
            goto numeric_range;
        case 'D':
            s = strptime(s, "%m/%d/%y", tm);
            if (!s) return 0;
            break;
        case 'F': {
            /* Width-limit the whole field but NOT the year subfield (musl). */
            i = 0;
            char tmp[20];
            if (*s == '-' || *s == '+') tmp[i++] = *s++;
            while (*s == '0' && isdigit((unsigned char)s[1])) s++;
            for (; *s && i < (size_t)w && i + 1 < (int)sizeof tmp; i++)
                tmp[i] = *s++;
            tmp[i] = 0;
            char *p = strptime(tmp, "%12Y-%m-%d", tm);
            if (!p) return 0;
            s -= tmp + i - p;
            break;
        }
        case 'H':
            dest = &tm->tm_hour;
            min = 0;
            range = 24;
            goto numeric_range;
        case 'I':
            dest = &tm->tm_hour;
            min = 1;
            range = 12;
            goto numeric_range;
        case 'j':
            dest = &tm->tm_yday;
            min = 1;
            range = 366;
            adj = 1;
            goto numeric_range;
        case 'm':
            dest = &tm->tm_mon;
            min = 1;
            range = 12;
            adj = 1;
            goto numeric_range;
        case 'M':
            dest = &tm->tm_min;
            min = 0;
            range = 60;
            goto numeric_range;
        case 'n': case 't':
            for (; *s && isspace((unsigned char)*s); s++);
            break;
        case 'p':
            if (!strncasecmp(s, "AM", 2)) { tm->tm_hour %= 12; s += 2; break; }
            if (!strncasecmp(s, "PM", 2)) { tm->tm_hour %= 12; tm->tm_hour += 12; s += 2; break; }
            return 0;
        case 'r':
            s = strptime(s, "%I:%M:%S %p", tm);
            if (!s) return 0;
            break;
        case 'R':
            s = strptime(s, "%H:%M", tm);
            if (!s) return 0;
            break;
        case 's':
            /* Parse only; effect on tm is unspecified (musl). */
            if (*s == '-') s++;
            if (!isdigit((unsigned char)*s)) return 0;
            while (isdigit((unsigned char)*s)) s++;
            break;
        case 'S':
            dest = &tm->tm_sec;
            min = 0;
            range = 61;
            goto numeric_range;
        case 'T':
            s = strptime(s, "%H:%M:%S", tm);
            if (!s) return 0;
            break;
        case 'U': case 'W':
            /* %U/%W/%V results are discarded (effect unspecified in musl). */
            dest = &dummy;
            min = 0;
            range = 54;
            goto numeric_range;
        case 'V':
            dest = &dummy;
            min = 1;
            range = 53;
            goto numeric_range;
        case 'g':
            dest = &dummy;
            w = 2;
            goto numeric_digits;
        case 'G':
            dest = &dummy;
            if (w < 0) w = 4;
            goto numeric_digits;
        case 'u':
            dest = &tm->tm_wday;
            min = 1;
            range = 7;
            goto numeric_range;
        case 'w':
            dest = &tm->tm_wday;
            min = 0;
            range = 7;
            goto numeric_range;
        case 'x':
            s = strptime(s, "%m/%d/%y", tm);
            if (!s) return 0;
            break;
        case 'X':
            s = strptime(s, "%H:%M:%S", tm);
            if (!s) return 0;
            break;
        case 'y':
            dest = &relyear;
            w = 2;
            want_century |= 1;
            goto numeric_digits;
        case 'Y':
            dest = &tm->tm_year;
            if (w < 0) w = 4;
            adj = 1900;
            want_century = 0;
            goto numeric_digits;
        case 'z':
            if (*s == '+') neg = 0;
            else if (*s == '-') neg = 1;
            else return 0;
            for (i = 0; i < 4; i++)
                if (!isdigit((unsigned char)s[1 + i])) return 0;
            tm->tm_gmtoff = (s[1] - '0') * 36000 + (s[2] - '0') * 3600
                          + (s[3] - '0') * 600 + (s[4] - '0') * 60;
            if (neg) tm->tm_gmtoff = -tm->tm_gmtoff;
            s += 5;
            break;
        case 'Z':
            __ensure_tz(); /* populate tzname before matching against it */
            if (!strncmp(s, tzname[0], len = strlen(tzname[0]))) {
                tm->tm_isdst = 0;
                s += len;
            } else if (!strncmp(s, tzname[1], len = strlen(tzname[1]))) {
                tm->tm_isdst = 1;
                s += len;
            } else {
                while (((*s) | 32) - 'a' <= 'z' - 'a') s++;
            }
            break;
        case '%':
            if (*s++ != '%') return 0;
            break;
        default:
            return 0;
        numeric_range:
            if (!isdigit((unsigned char)*s)) return 0;
            *dest = 0;
            for (i = 1; i <= min + range && isdigit((unsigned char)*s); i *= 10)
                *dest = *dest * 10 + *s++ - '0';
            if (*dest - min >= (unsigned)range) return 0;
            *dest -= adj;
            goto update;
        numeric_digits:
            neg = 0;
            if (*s == '+') s++;
            else if (*s == '-') neg = 1, s++;
            if (!isdigit((unsigned char)*s)) return 0;
            for (*dest = i = 0; i < w && isdigit((unsigned char)*s); i++)
                *dest = *dest * 10 + *s++ - '0';
            if (neg) *dest = -*dest;
            *dest -= adj;
            goto update;
        update:
            ;
        }
    }
    if (want_century) {
        tm->tm_year = relyear;
        if (want_century & 2) tm->tm_year += century * 100 - 1900;
        else if (tm->tm_year <= 68) tm->tm_year += 100;
    }
    return (char *)s;
}
