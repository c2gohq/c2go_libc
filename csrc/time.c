/* c2go-libc2 time: the parts of <time.h> that are NOT musl's algorithm, so they
 * are our own implementation here rather than an in-place musl patch (musl's own
 * src/time/*.c keep the pure calendar/format code: gmtime, strftime, __secs_to_tm,
 * ...). Three groups, each a wholesale replacement of what musl does:
 *
 *   - clock / sleep: musl reads the clock via vDSO/syscall and parks via
 *     clock_nanosleep; c2go-compiled C cannot syscall, so both go through the Go
 *     time bridge (time.go). The c2go-uniform clock ids are interpreted in Go.
 *   - timezone: musl parses a zoneinfo DB (/etc/localtime, $TZ); Go's time.Local
 *     is the single source of truth shared with Go's own time package, so the
 *     whole parser is replaced by a snapshot from __c2go_tzset.
 *   - localtime / mktime: musl walks the zoneinfo DB (__secs_to_zone); here the
 *     per-instant LOCAL offset comes from time.Local via __c2go_local_offset /
 *     __c2go_mktime, and the shared musl UTC calendar math (__secs_to_tm) runs on
 *     the shifted seconds.
 *
 * See CMakeLists: these symbols are excluded from C2GO_MUSL_SOURCES. */
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <c2go.h>

/* Go bridges (time.go): read a clock / park / resolve the LOCAL zone. These ARE
 * real Go functions, so they carry the GoABI0 target-ABI selector. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_clock_gettime", C2GO_GOABI0)
int __c2go_clock_gettime(int id, long long *sec, long long *nsec);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_nanosleep", C2GO_GOABI0)
void __c2go_nanosleep(long long sec, long long nsec);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_local_offset", C2GO_GOABI0)
void __c2go_local_offset(long long sec, long long *off, int *isdst, const char **zone);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_mktime", C2GO_GOABI0)
long long __c2go_mktime(int year, int mon, int mday, int hour, int min, int sec,
                        long long *off, int *isdst, const char **zone);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_tzset", C2GO_GOABI0)
void __c2go_tzset(int reload, const char **std, const char **dst, long long *stdwest, int *hasdst);

/* __secs_to_tm: musl's UTC calendar helper (src/time/__secs_to_tm.c), called
 * cross-TU here. The C2GO_FUNC world bit marks the reference package-internal
 * (not a host import), keeping the default internal calling convention. */
#pragma c2go managed(1) push
int __secs_to_tm(long long, struct tm *);
#pragma c2go pop

/* ---- clock / sleep: Go time bridge ------------------------------------- */

c2go_extern int clock_gettime(clockid_t id, struct timespec *ts)
{
	long long s, n;
	int r = __c2go_clock_gettime(id, &s, &n);
	if (r != 0) { errno = r; return -1; }
	if (ts) { ts->tv_sec = s; ts->tv_nsec = n; }
	return 0;
}

c2go_extern int clock_getres(clockid_t id, struct timespec *ts)
{
	/* Validate id against the set clock_gettime accepts (single source of truth
	 * in time.go); nominal 1ns resolution. */
	long long s, n;
	int r = __c2go_clock_gettime(id, &s, &n);
	if (r != 0) { errno = r; return -1; }
	if (ts) { ts->tv_sec = 0; ts->tv_nsec = 1; }
	return 0;
}

c2go_extern clock_t clock(void)
{
	/* No process-CPU clock under the Go runtime; approximate with monotonic
	 * process-elapsed time. CLOCKS_PER_SEC == 1000000, so return microseconds. */
	long long s, n;
	__c2go_clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s, &n);
	return (clock_t)(s * 1000000 + n / 1000);
}

c2go_extern time_t time(time_t *t)
{
	long long s, n;
	__c2go_clock_gettime(CLOCK_REALTIME, &s, &n);
	if (t) *t = (time_t)s;
	return (time_t)s;
}

c2go_extern int gettimeofday(struct timeval *tv, void *tz)
{
	(void)tz; /* the obsolete struct timezone is ignored */
	long long s, n;
	__c2go_clock_gettime(CLOCK_REALTIME, &s, &n);
	if (tv) { tv->tv_sec = s; tv->tv_usec = n / 1000; }
	return 0;
}

c2go_extern int timespec_get(struct timespec *ts, int base)
{
	if (base != TIME_UTC) return 0;
	return clock_gettime(CLOCK_REALTIME, ts) == 0 ? base : 0;
}

c2go_extern int nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999) {
		errno = EINVAL;
		return -1;
	}
	__c2go_nanosleep(req->tv_sec, req->tv_nsec);
	if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; } /* no early wakeup */
	return 0;
}

/* ---- timezone: snapshot of Go's time.Local ----------------------------- */

/* tzname/timezone/daylight exist only for programs that read them; localtime/
 * mktime query the per-instant offset directly and do NOT depend on these.
 * tzname points at process-stable interned zone names (time.go keeps them alive
 * -- non-zero-init C globals, so not #646-ceded GC roots). */
char *tzname[2] = { (char *)"UTC", (char *)"UTC" };
long  timezone = 0;
int   daylight = 0;

/* reload=1 (public tzset) first re-reads $TZ into time.Local; reload=0 (the lazy
 * ensure) only snapshots and never writes time.Local. */
static void __tz_refresh(int reload)
{
	const char *std, *dst;
	long long west;
	int hasdst;
	__c2go_tzset(reload, &std, &dst, &west, &hasdst);
	tzname[0] = (char *)std;
	tzname[1] = (char *)dst;
	timezone = (long)west;
	daylight = hasdst;
}

c2go_extern void tzset(void)
{
	__tz_refresh(1);
}

/* One-time lazy snapshot for localtime/mktime/strptime. Refreshing on FIRST use
 * only fixes the pre-tzset "UTC" placeholder; a later zone change still needs an
 * explicit tzset() (POSIX). The check-then-set race is benign -- a redundant
 * snapshot is idempotent. */
static int __tz_ready = 0;
void __ensure_tz(void)
{
	if (!__tz_ready) {
		__tz_ready = 1;
		__tz_refresh(0);
	}
}

/* ---- localtime / mktime: per-instant offset from the Go bridge ---------- */

c2go_extern struct tm *localtime_r(const time_t *restrict t, struct tm *restrict tm)
{
	long long off; int isdst; const char *zone;
	__ensure_tz();
	__c2go_local_offset((long long)*t, &off, &isdst, &zone);
	if (__secs_to_tm((long long)*t + off, tm) < 0) {
		errno = EOVERFLOW;
		return 0;
	}
	tm->tm_isdst = isdst;
	tm->tm_gmtoff = off;
	tm->tm_zone = zone;
	return tm;
}

c2go_extern time_t mktime(struct tm *tm)
{
	long long off; int isdst; const char *zone;
	__ensure_tz();
	long long u = __c2go_mktime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	                            tm->tm_hour, tm->tm_min, tm->tm_sec, &off, &isdst, &zone);
	if (__secs_to_tm(u + off, tm) < 0) {
		errno = EOVERFLOW;
		return -1;
	}
	tm->tm_isdst = isdst;
	tm->tm_gmtoff = off;
	tm->tm_zone = zone;
	return (time_t)u;
}

/* ---- asctime_r: C-locale broken-down time -> fixed 26-byte string ------
 * musl's asctime_r resolves the day/month abbreviations via __nl_langinfo_l; the
 * langinfo/locale cluster is not yet built in c2go-libc2, and asctime is a fixed
 * C-locale (ISO C) format, so the abbreviations are inlined here. Once the locale
 * cluster migrates this can revert to musl's src/time/asctime_r.c verbatim. Out of
 * range wday/mon -> "" (as musl's langinfo returns for those); a field too large
 * for the 26-byte buffer traps, matching musl's deliberate a_crash. */
c2go_extern char *asctime_r(const struct tm *restrict tm, char *restrict buf)
{
	static const char *const wday[7] =
		{ "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
	static const char *const mon[12] =
		{ "Jan","Feb","Mar","Apr","May","Jun",
		  "Jul","Aug","Sep","Oct","Nov","Dec" };
	const char *wd = (unsigned)tm->tm_wday < 7 ? wday[tm->tm_wday] : "";
	const char *mo = (unsigned)tm->tm_mon < 12 ? mon[tm->tm_mon] : "";
	if (snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
		wd, mo, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
		1900 + tm->tm_year) >= 26)
		__builtin_trap();
	return buf;
}
