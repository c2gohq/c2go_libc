/* sys/resource.h — resource usage/limits (#675 C wave 2b). Impl
 * source/resource.c over the Go bridges (syscall.Getrusage/Getrlimit).
 *
 * struct rusage / struct rlimit are the UNIFORM musl shapes — the bridge
 * fills them field-by-field from the host structs, never a per-OS memcpy.
 * The RLIMIT_* selector VALUES are the target's native ones (they go
 * straight into the kernel call): linux = musl, darwin = xnu — note
 * RLIMIT_NOFILE is 7 vs 8 and darwin folds RSS into AS. Field SEMANTICS
 * stay kernel-native and are deliberately not normalized (documented):
 * ru_maxrss is KiB on linux but BYTES on darwin, and darwin leaves most
 * non-maxrss ru_* fields zero. RLIM_INFINITY likewise differs (~0ULL vs
 * 2^63-1) and passes through unmapped — both sides of a get/compare use
 * the same header, so the representation is self-consistent.
 * Only the implemented surface is declared: getrusage + getrlimit
 * (setrlimit/getpriority/prlimit deliberately absent until implemented). */
#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#if defined(_WIN32)
#error "<sys/resource.h> is not available on Windows (no MinGW/CRT counterpart; #677 audit)"
#endif

#include <c2go.h>

#define __NEED_time_t
#define __NEED_suseconds_t
#define __NEED_struct_timeval

#include <bits/alltypes.h>

typedef unsigned long long rlim_t;

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
	/* linux extensions, but useful (musl keeps them unconditionally) */
	long	ru_maxrss;
	long	ru_ixrss;
	long	ru_idrss;
	long	ru_isrss;
	long	ru_minflt;
	long	ru_majflt;
	long	ru_nswap;
	long	ru_inblock;
	long	ru_oublock;
	long	ru_msgsnd;
	long	ru_msgrcv;
	long	ru_nsignals;
	long	ru_nvcsw;
	long	ru_nivcsw;
	/* room for more... */
	long    __reserved[16];
};

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)

#if defined(__APPLE__)
/* xnu sys/resource.h */
#define RLIM_INFINITY   (((rlim_t)1 << 63) - 1)
#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_AS       5
#define RLIMIT_RSS      RLIMIT_AS
#define RLIMIT_MEMLOCK  6
#define RLIMIT_NPROC    7
#define RLIMIT_NOFILE   8
#else
/* musl include/sys/resource.h (linux) */
#define RLIM_INFINITY   (~0ULL)
#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_RSS      5
#define RLIMIT_NPROC    6
#define RLIMIT_NOFILE   7
#define RLIMIT_MEMLOCK  8
#define RLIMIT_AS       9
#endif

#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

int getrlimit(int, struct rlimit *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getrlimit", C2GO_GOABI0);
int getrusage(int, struct rusage *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getrusage", C2GO_GOABI0);

#endif /* _SYS_RESOURCE_H */
