/* sched.h — minimal POSIX scheduling surface (#675): sched_yield only, backed
 * by runtime.Gosched (cooperative reschedule is the only meaningful yield
 * under the Go scheduler). Priority/affinity APIs are deliberately absent
 * (documented omission — they cannot be honored for goroutines). */
#ifndef _SCHED_H
#define _SCHED_H

#include <c2go.h>

int sched_yield(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.sched_yield", C2GO_GOABI0);

#endif /* _SCHED_H */
