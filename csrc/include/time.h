/* time.h — time and date. time_t/clock_t/struct timespec come from the type
 * pool; struct tm is defined here. */
#ifndef _TIME_H
#define _TIME_H

#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_size_t
#define __NEED_struct_timespec
#define __NEED_locale_t
#include <bits/alltypes.h>
#include <c2go.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define CLOCKS_PER_SEC 1000000L
#define TIME_UTC 1

/* c2go clock ids — interpreted by the Go clock bridge (time.Now / runtime
 * nanotime), NOT host clock_gettime ids, so they are uniform across targets. */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

struct tm {
	int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
	int tm_wday, tm_yday, tm_isdst;
	long tm_gmtoff;
	const char *tm_zone;
};

clock_t clock(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.clock", C2GO_GOABI0);
time_t time(time_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.time", C2GO_GOABI0);
double difftime(time_t, time_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.difftime", C2GO_GOABI0);
time_t mktime(struct tm *)
    c2go_linkname("github.com/c2gohq/c2go_libc.mktime", C2GO_GOABI0);
time_t timegm(struct tm *)
    c2go_linkname("github.com/c2gohq/c2go_libc.timegm", C2GO_GOABI0);
size_t strftime(char *__restrict, size_t, const char *__restrict, const struct tm *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strftime", C2GO_GOABI0);
/* one locale, so strftime_l ignores its locale_t (musl's __strftime_l localizes
 * month/day names via message catalogs; with none installed it is the C output). */
size_t strftime_l(char *__restrict, size_t, const char *__restrict, const struct tm *__restrict, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strftime_l", C2GO_GOABI0);
char *strptime(const char *__restrict, const char *__restrict, struct tm *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strptime", C2GO_GOABI0);

struct tm *gmtime(const time_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.gmtime", C2GO_GOABI0);
struct tm *localtime(const time_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.localtime", C2GO_GOABI0);
struct tm *gmtime_r(const time_t *__restrict, struct tm *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.gmtime_r", C2GO_GOABI0);
struct tm *localtime_r(const time_t *__restrict, struct tm *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.localtime_r", C2GO_GOABI0);

/* tzset re-syncs the local zone with Go's time.Local (the shared source of truth)
 * and refreshes the globals below. tzname/timezone/daylight are plain externs —
 * c2go links them across TUs by symbol name (like stdout / optarg). */
void tzset(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.tzset", C2GO_GOABI0);
extern char *tzname[2];
extern long timezone;
extern int daylight;

char *asctime(const struct tm *)
    c2go_linkname("github.com/c2gohq/c2go_libc.asctime", C2GO_GOABI0);
char *ctime(const time_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ctime", C2GO_GOABI0);
char *asctime_r(const struct tm *__restrict, char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.asctime_r", C2GO_GOABI0);
char *ctime_r(const time_t *, char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.ctime_r", C2GO_GOABI0);

int clock_gettime(clockid_t, struct timespec *)
    c2go_linkname("github.com/c2gohq/c2go_libc.clock_gettime", C2GO_GOABI0);
int clock_getres(clockid_t, struct timespec *)
    c2go_linkname("github.com/c2gohq/c2go_libc.clock_getres", C2GO_GOABI0);
int nanosleep(const struct timespec *, struct timespec *)
    c2go_linkname("github.com/c2gohq/c2go_libc.nanosleep", C2GO_GOABI0);
int timespec_get(struct timespec *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.timespec_get", C2GO_GOABI0);

#endif /* _TIME_H */
