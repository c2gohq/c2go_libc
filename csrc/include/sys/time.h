/* sys/time.h — struct timeval + gettimeofday (served by the Go clock bridge). */
#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#define __NEED_time_t
#define __NEED_suseconds_t
#define __NEED_struct_timeval
#include <bits/alltypes.h>
#include <c2go.h>

struct timezone { int tz_minuteswest; int tz_dsttime; };  /* obsolete; ignored */

int gettimeofday(struct timeval *, void *)
    c2go_linkname("github.com/c2gohq/c2go_libc.gettimeofday", C2GO_GOABI0);

#if !defined(_WIN32)
/* legacy microsecond face over utimensat (source/stat2.c, musl shape) */
int utimes(const char *, const struct timeval [2])
    c2go_linkname("github.com/c2gohq/c2go_libc.utimes", C2GO_GOABI0);
#endif

#endif /* _SYS_TIME_H */
