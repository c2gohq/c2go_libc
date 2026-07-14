/* signal.h — signal handling (C-standard core). Signal delivery is limited
 * under the Go runtime; the Go os/signal bridge handles what it can. */
#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <bits/signal.h>   /* SIG* numbers (per-OS native) */
#include <c2go.h>          /* c2go_linkname + C2GO_GOABI0 for the exported surface */

typedef int sig_atomic_t;

/* handler sentinels — the same on all targets */
#define SIG_ERR ((void (*)(int))-1)
#define SIG_DFL ((void (*)(int)) 0)
#define SIG_IGN ((void (*)(int)) 1)

/* Both are package-provided C wrappers (source/signal.c) on every target: the
 * handler table lives Go-side as atomics and deliveries ride the unified
 * os/signal pump (Windows included; the CRT-backed path is retired — #642).
 * signal() is limited under the Go runtime — see the source header. */
void (*signal(int, void (*)(int)))(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.signal", C2GO_GOABI0);
int  raise(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.raise", C2GO_GOABI0);
/* "msg: <signal name>" to stderr (musl signal/psignal.c; impl in
 * source/stdio.c — it manipulates FILE internals under FLOCK). Unix-only
 * with strsignal. */
#if !defined(_WIN32)
void psignal(int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.psignal", C2GO_GOABI0);
#endif

/* kill (#675): cross-process signalling / existence probe (kill(pid, 0)).
 * Unix-only (MinGW has no kill); self-signalling should use raise(). */
#if !defined(_WIN32)
int kill(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.Kill", C2GO_GOABI0);
#endif

/* NOTE: sigprocmask remains deferred (needs a real mask model under the Go
 * runtime); sigaction's restricted form + the sigset ops live below (#654). */


/* ── sigset + limited sigaction (#654, Top10-2) ──────────────────────────────
 * sigset_t is a c2go-uniform 64-bit mask (signals 1..63) — there are no
 * per-thread kernel masks under the Go runtime, so sa_mask is accepted and
 * recorded but never applied (the Go runtime owns real signal masking).
 * sigaction is signal() in structural clothing: sa_handler dispositions only;
 * SA_SIGINFO is refused honestly (EINVAL). SA_RESTART is accepted as a no-op
 * (the runtime restarts syscalls itself). */
typedef unsigned long long sigset_t;

int sigemptyset(sigset_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigemptyset", C2GO_GOABI0);
int sigfillset(sigset_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigfillset", C2GO_GOABI0);
int sigaddset(sigset_t *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigaddset", C2GO_GOABI0);
int sigdelset(sigset_t *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigdelset", C2GO_GOABI0);
int sigismember(const sigset_t *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigismember", C2GO_GOABI0);

#define SA_SIGINFO 4
#define SA_RESTART 0x10000000

struct sigaction {
	void (*sa_handler)(int);
	sigset_t sa_mask;
	int sa_flags;
};

int sigaction(int, const struct sigaction *__restrict, struct sigaction *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.sigaction", C2GO_GOABI0);

#endif /* _SIGNAL_H */
