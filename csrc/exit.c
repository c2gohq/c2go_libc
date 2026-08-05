/* exit.c — process control: exit / atexit / abort / assert (split out of the
 * OLD stdlib.c bucket; the number-conversion/arith parts of that bucket are
 * fork-per-file musl now). The atexit registry AND its handler invocation stay
 * in C so the registered c2go function pointers are called natively; only the
 * final process stop crosses into Go (os.Exit is the one way to halt the Go
 * runtime). See the section note below. */
#include <stdio.h>  /* exit() flushes via fflush(NULL); __assert_fail uses stderr */
#include <stdlib.h>
#include <c2go.h>

/* ── process control: exit / atexit / abort / assert ─────────────────────────
 * The atexit registry AND its handler invocation stay in C so the registered
 * c2go function pointers are called natively; only the final process stop crosses
 * into Go (os.Exit is the one way to halt the Go runtime). C99 guarantees at
 * least 32 registered handlers; beyond that atexit returns nonzero. Under the Go
 * runtime any goroutine may call atexit concurrently, so the registry is guarded
 * by a Go mutex (process.go's atexitMu, via _c2go_atexit_lock/unlock); the drain
 * pops each handler under the lock and runs it UNLOCKED so a handler may safely
 * re-register. */

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_exit", C2GO_GOABI0)
_Noreturn void __c2go_exit(int code);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_abort", C2GO_GOABI0)
_Noreturn void __c2go_abort(void);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_atexit_lock", C2GO_GOABI0)
void _c2go_atexit_lock(void);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_atexit_unlock", C2GO_GOABI0)
void _c2go_atexit_unlock(void);
c2go_linkname("github.com/c2gohq/c2go_libc._c2go_run_finalize_hooks", C2GO_GOABI0)
void _c2go_run_finalize_hooks(void);

#define ATEXIT_MAX 32
static void (*__atexit_funcs[ATEXIT_MAX])(void);
static int  __atexit_n;
static void (*__at_quick_funcs[ATEXIT_MAX])(void);
static int  __at_quick_n;

c2go_extern int atexit(void (*f)(void)) {
    _c2go_atexit_lock();
    int r = -1;
    if (__atexit_n < ATEXIT_MAX) { __atexit_funcs[__atexit_n++] = f; r = 0; }
    _c2go_atexit_unlock();
    return r;
}

c2go_extern int at_quick_exit(void (*f)(void)) {
    _c2go_atexit_lock();
    int r = -1;
    if (__at_quick_n < ATEXIT_MAX) { __at_quick_funcs[__at_quick_n++] = f; r = 0; }
    _c2go_atexit_unlock();
    return r;
}

/* __c2go_finalize runs the registered atexit handlers (LIFO) and flushes every
 * open stream, WITHOUT terminating the process — the shared shutdown core of
 * exit(). It is a GoABI0 export (Go binding C2goFinalize) so a hand-written Go
 * host — whose own func main, not the c2go-generated entry wrapper, owns process
 * termination — can run C's exit hooks + flush buffered stdio (e.g. via defer)
 * before its own os.Exit. Idempotent: the pop-drain empties the registry and
 * fflush(NULL) on an already-flushed stream is a no-op, so a later exit() or a
 * second call does nothing further. */
c2go_extern void __c2go_finalize(void) {
    /* Pop each handler under the lock, call it unlocked (LIFO) so a handler that
     * itself calls atexit does not self-deadlock on the non-recursive mutex. */
    for (;;) {
        _c2go_atexit_lock();
        void (*f)(void) = __atexit_n > 0 ? __atexit_funcs[--__atexit_n] : 0;
        _c2go_atexit_unlock();
        if (!f) break;
        f();
    }
    /* Subpackages flush first, then root libc's own FILE world. Hooks are Go
     * functions rather than C atexit slots, so they do not consume the C99
     * minimum registry capacity and cannot be missed when that registry is
     * full. */
    _c2go_run_finalize_hooks();
    fflush(NULL);                        /* flush every root-libc stream */
}

c2go_extern _Noreturn void exit(int code) {
    __c2go_finalize();                   /* run atexit handlers, then flush stdio */
    __c2go_exit(code);
}

/* _Exit is immediate termination (no atexit, no flush) == the __c2go_exit shim
 * itself, so <stdlib.h> linknames _Exit straight to it (no C wrapper — and it
 * dodges the c2go-bind name clash where _Exit and exit both fold to Go "Exit"). */

c2go_extern _Noreturn void quick_exit(int code) {
    for (;;) {
        _c2go_atexit_lock();
        void (*f)(void) = __at_quick_n > 0 ? __at_quick_funcs[--__at_quick_n] : 0;
        _c2go_atexit_unlock();
        if (!f) break;
        f();
    }
    __c2go_exit(code);
}

/* signal.c's synchronous SIGABRT dispatch: POSIX abort() runs a caught handler
 * once before termination, and abort never returns, so the delivery cannot
 * ride the async os/signal pump — signal.c runs it in the calling thread
 * (one-shot: the slot is reset first). Cross-TU: linkname declaration paired
 * with signal.c's c2go_extern(C2GO_KEEPCASE) definition (a bare extern would
 * be an unmanaged HOST import under the default-unmanaged model — the #672
 * c2go-lto guard rejects that mix). */
void __c2go_signal_dispatch_abort(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_signal_dispatch_abort", C2GO_GOABI0);

c2go_extern _Noreturn void abort(void) {
    __c2go_signal_dispatch_abort();  /* run a caught SIGABRT handler, once */
    __c2go_abort();                  /* then terminate regardless (os.Exit(134)) */
}

/* 4th param is 'fn' not 'func' — c2go-bind copies C parameter names verbatim into
 * the Go binding, and 'func' is a Go keyword (would break the generated .go). */
c2go_extern _Noreturn void __assert_fail(const char *expr, const char *file,
                                         int line, const char *fn) {
    fprintf(stderr, "Assertion failed: %s (%s: %s: %d)\n", expr, file, fn, line);
    abort();
}
