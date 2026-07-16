/* signal_selftest.c — in-C exercise of signal()/raise() (Unix), driven from Go
 * via the SignalSelftest* entry points.
 *
 * It installs a custom SIGUSR1 handler (which starts the os/signal pump in
 * ../../signal.go), self-raises SIGUSR1, and lets Go poll the flag the handler
 * sets — the handler runs on the pump goroutine, so delivery is asynchronous. It
 * also checks that signal() honestly refuses a synchronous hardware fault
 * (SIGSEGV, owned by the Go runtime) with SIG_ERR, and that a SIG_IGN install
 * reports the prior SIG_DFL. Reached FROM C across a TU boundary, so it also
 * exercises the cross-TU c2go_linkname calls into signal()/raise(). */
#include <signal.h>
#include <errno.h>
#include <stdlib.h>  /* abort */
#include <unistd.h>  /* write */

/* The signal number the handler last saw; volatile because it is written on the
 * pump goroutine and polled from the test goroutine (a benign write-once flag,
 * the canonical signal-handler pattern). */
static volatile int g_fired;

static void on_sig(int sig) { g_fired = sig; }

c2go_extern void SignalSelftestInstall(void) {
    g_fired = 0;
    signal(SIGUSR1, on_sig);
}

c2go_extern int SignalSelftestRaise(void) {
    return raise(SIGUSR1);
}

c2go_extern int SignalSelftestFired(void) {
    return g_fired;
}

/* signal() must refuse SIGSEGV (Go turns it into a panic) with SIG_ERR + EINVAL. */
c2go_extern int SignalSelftestSegvRefused(void) {
    errno = 0;
    return signal(SIGSEGV, on_sig) == SIG_ERR && errno == EINVAL;
}

/* First SIG_IGN install for SIGUSR2 returns the prior disposition, SIG_DFL. */
c2go_extern int SignalSelftestIgn(void) {
    return signal(SIGUSR2, SIG_IGN) == SIG_DFL;
}

/* abort() must run a caught SIGABRT handler ONCE, synchronously, before dying
 * (POSIX; the async os/signal pump can't carry it — abort never returns). The
 * handler writes one byte straight to fd 1 (unbuffered syscall), so the parent
 * of the re-exec'd child (see ../signal_test.go) asserts stdout=="H" plus the
 * abort exit status. */
static void on_abrt(int sig) { (void)sig; write(1, "H", 1); }

c2go_extern void SignalSelftestAbort(void) {
    signal(SIGABRT, on_abrt);
    abort();
}
