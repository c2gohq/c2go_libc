/* signal.c — raise() and a limited signal() under the Go runtime.
 *
 * WHY signal handling is limited here: the Go runtime installs its OWN handlers
 * for the whole process (async preemption via SIGURG, panic translation for the
 * synchronous hardware faults SIGSEGV/SIGILL/SIGFPE/SIGBUS, etc.; on Windows a
 * console ctrl handler + VEH). c2go-libc must NOT install raw sigaction/CRT
 * handlers over those — it would break the runtime. So signal() cooperates with
 * the runtime instead of fighting it, ON EVERY TARGET:
 *
 *   - A custom handler is delivered through Go's os/signal (../signal.go),
 *     which multiplexes on top of the runtime's handler. On delivery the pump
 *     goroutine calls back here (__c2go_signal_run) to run the stored C handler
 *     — IN C, the "call the fp in C" pattern of qsort.c / pthread.c — so the
 *     handler runs on an ordinary goroutine, NOT in real async-signal context.
 *     That is the honest limit; a handler that only sets a flag or does cleanup
 *     (the common case) works, and it is actually safer than true signal context.
 *     The synchronous hardware faults are refused (SIG_ERR) rather than silently
 *     mishandled — Go owns those.
 *
 *   - Windows delivers through the SAME pump: the Go runtime maps Ctrl+C/Break →
 *     SIGINT and console close/logoff/shutdown → SIGTERM into os/signal. The old
 *     CRT-backed signal() (which had to refuse custom handlers — the CRT would
 *     call a GoABI0 fp as cdecl) is gone: dispatch happens in OUR C world, so
 *     the ABI objection evaporates and custom handlers work. SIGABRT & co stay
 *     raise()/abort()-only — exactly the CRT's own reality.
 *
 *   - raise(): unix self-delivers via the kernel (../raise_*.go) so the HOST's
 *     os/signal subscribers see it too; Windows has no kernel signals — its CRT
 *     raise is likewise a synchronous in-process dispatch, mirrored here against
 *     our table (SIG_DFL emulates the CRT default: terminate, exit code 3).
 *
 * The sig->handler table and all dispatch live in C; Go only ever sees an int
 * signal number, so no function pointer crosses the boundary and the GC is never
 * exposed to a handler value. The table is file-local plain data holding only
 * code addresses (never heap pointers), so it needs no GC-root treatment.
 */
#include <c2go.h>
#include <signal.h>   /* SIG_* numbers + SIG_ERR/SIG_DFL/SIG_IGN sentinels */
#include <errno.h>    /* errno + EINVAL (per-OS native) */
#include <stdlib.h>   /* _Exit — the Windows raise() SIG_DFL terminator */

/* One slot per signal number; large enough for every target's max (Linux/macOS
 * 31, Windows 22). The slots hold SIG_DFL(0) / SIG_IGN(1) / a c2go handler fp,
 * and live on the GO side as atomics (#658 M13): signal() swaps from the
 * calling goroutine while the os/signal pump loads per delivery — a plain C
 * array was a C/Go data race. A handler fp is a code address, so it travels
 * as a plain integer (the managed model's fp==uintptr rule). */
#define C2GO_NSIG 33

static int sig_ok(int sig) { return sig > 0 && sig < C2GO_NSIG; }

/* Go bridge (../signal.go, portable): manage OUR os/signal channel per signal.
 * watch/ignore both keep the pump registered (the table decides per delivery);
 * default detaches only our channel — never process-wide Ignore/Reset, which
 * would also unhook the Go HOST's own subscriptions. */
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_watch", C2GO_GOABI0)
void __c2go_signal_watch(int sig);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_ignore", C2GO_GOABI0)
void __c2go_signal_ignore(int sig);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_default", C2GO_GOABI0)
void __c2go_signal_default(int sig);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_swap", C2GO_GOABI0)
unsigned long long __c2go_signal_swap(int sig, unsigned long long h);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_load", C2GO_GOABI0)
unsigned long long __c2go_signal_load(int sig);
#if !defined(_WIN32)
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_raise", C2GO_GOABI0)
int  __c2go_raise(int sig);
#endif

/* The os/signal pump (../signal.go) calls this on each delivery of `sig`; it runs
 * the stored handler here in C. Kept a void-returning fixed function (not an fp
 * handed to Go) so nothing but an int ever crosses the boundary. */
c2go_extern void __c2go_signal_run(int sig, unsigned long long h) {
	if (sig_ok(sig) && h > 1) /* 0 = SIG_DFL, 1 = SIG_IGN */
		((void (*)(int))h)(sig);
}

/* abort()'s synchronous SIGABRT dispatch (called by exit.c's abort): POSIX
 * runs a caught handler once before the process dies, and abort() never
 * returns, so the delivery cannot ride the async os/signal pump. One-shot: the
 * slot is reset first, so a handler that re-aborts just terminates (musl's
 * raise-then-default-re-raise shape). Cross-TU surface: KEEPCASE export
 * (c2go_extern_as) paired with exit.c's linkname declaration (#672 guard). */
c2go_extern_as(C2GO_KEEPCASE)
void __c2go_signal_dispatch_abort(void) {
	unsigned long long h = __c2go_signal_swap(SIGABRT, 0); /* one-shot reset */
	if (h > 1)
		((void (*)(int))h)(SIGABRT);
}

/* Signals the Go runtime handles itself (synchronous hardware faults it turns
 * into panics — via sigaction on unix, VEH on Windows). Catching them would
 * displace the runtime's handler, so signal() refuses them honestly with
 * SIG_ERR rather than half-working. */
static int uncatchable(int sig) {
	switch (sig) {
	case SIGSEGV: case SIGILL: case SIGFPE:
#if !defined(_WIN32)
	case SIGBUS: case SIGTRAP:
#endif
		return 1;
	default:
		return 0;
	}
}

c2go_extern void (*signal(int sig, void (*h)(int)))(int) {
	if (!sig_ok(sig) || h == SIG_ERR || uncatchable(sig)) {
		errno = EINVAL;
		return SIG_ERR;
	}
	/* #654c-b: keep the previous handler as an INTEGER while it lives across
	   the dispatch calls below. A function-pointer local spilled across a call
	   can be GC-marked, and its value can be the SIG_IGN sentinel (1) — a
	   marked stack word holding a small non-zero integer makes copystack throw
	   "invalid pointer found on stack". */
	unsigned long long old = __c2go_signal_swap(sig, (unsigned long long)h);
	if (h == SIG_IGN)
		__c2go_signal_ignore(sig);
	else if (h == SIG_DFL)
		__c2go_signal_default(sig);
	else
		__c2go_signal_watch(sig);
	return old ? (void (*)(int))old : SIG_DFL;
}

#if !defined(_WIN32)
/* Unix raise: kernel self-delivery (tgkill / kill(getpid)) — the runtime's
 * handler catches it and os/signal fans it out, so the Go HOST's subscribers
 * observe the raise exactly like an external signal. */
c2go_extern int raise(int sig) {
	int r = __c2go_raise(sig);
	if (r < 0) { errno = -r; return -1; }
	return 0;
}
#else
/* Windows raise: no kernel signals exist — the CRT's raise is itself a
 * synchronous in-process dispatch, so mirror it against OUR table: run a
 * custom handler in the calling thread (unlike the CRT we call it at GoABI0,
 * which is what a c2go fp is); SIG_DFL emulates the CRT default action,
 * terminating with exit code 3. OS events (Ctrl+C → SIGINT, console close →
 * SIGTERM) still arrive asynchronously via the os/signal pump. */
c2go_extern int raise(int sig) {
	if (!sig_ok(sig)) { errno = EINVAL; return -1; }
	unsigned long long h = __c2go_signal_load(sig);
	if (h == 1) /* SIG_IGN */
		return 0;
	if (h > 1) {
		((void (*)(int))h)(sig);
		return 0;
	}
	_Exit(3);
}
#endif

/* ── sigset bit ops (#654, musl signal/sigsetops shape on the 64-bit model) ── */
c2go_extern int sigemptyset(sigset_t *set) { *set = 0; return 0; }
c2go_extern int sigfillset(sigset_t *set) { *set = ~0ULL; return 0; }
c2go_extern int sigaddset(sigset_t *set, int sig) {
	if (!sig_ok(sig)) { errno = EINVAL; return -1; }
	*set |= 1ULL << sig;
	return 0;
}
c2go_extern int sigdelset(sigset_t *set, int sig) {
	if (!sig_ok(sig)) { errno = EINVAL; return -1; }
	*set &= ~(1ULL << sig);
	return 0;
}
c2go_extern int sigismember(const sigset_t *set, int sig) {
	if (!sig_ok(sig)) { errno = EINVAL; return -1; }
	return (*set >> sig) & 1;
}

/* sigaction (#654): signal() in structural clothing — handler dispositions
 * over the same Go atomic table + pump; sa_mask recorded-not-applied (header
 * note), SA_SIGINFO refused (EINVAL), query (act==NULL) is a pure load. */
c2go_extern int sigaction(int sig, const struct sigaction *restrict act,
                          struct sigaction *restrict old)
{
	if (!sig_ok(sig)) { errno = EINVAL; return -1; }
	unsigned long long oldh;
	if (act) {
		/* #654c-b: hold the handler as an INTEGER across the calls below —
		   see signal() above (SIG_IGN sentinel vs GC-marked spill slot). */
		unsigned long long h = (unsigned long long)act->sa_handler;
		if ((act->sa_flags & SA_SIGINFO) ||
		    (void (*)(int))h == SIG_ERR || uncatchable(sig)) {
			errno = EINVAL;
			return -1;
		}
		oldh = __c2go_signal_swap(sig, h);
		if ((void (*)(int))h == SIG_IGN)
			__c2go_signal_ignore(sig);
		else if ((void (*)(int))h == SIG_DFL)
			__c2go_signal_default(sig);
		else
			__c2go_signal_watch(sig);
	} else {
		oldh = __c2go_signal_load(sig);
	}
	if (old) {
		old->sa_handler = (void (*)(int))oldh; /* 0/1 == SIG_DFL/SIG_IGN */
		old->sa_mask = 0;
		old->sa_flags = 0;
	}
	return 0;
}
