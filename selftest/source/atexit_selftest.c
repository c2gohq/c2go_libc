/* atexit_selftest.c — in-C exercise of atexit() LIFO ordering and __assert_fail,
 * driven from Go via AtexitSelftestLIFO() / AtexitSelftestAssert(). Both entry
 * points deliberately terminate the process (exit()/abort() → os.Exit), so the
 * Go side runs each in a re-exec'd child and inspects its output + exit status
 * (see ../atexit_test.go).
 *
 *   - AtexitSelftestLIFO registers three handlers, each of which writes one
 *     distinct byte straight to fd 1 with write() — an unbuffered syscall, so
 *     the bytes land on stdout in exactly the order the handlers fire. exit(0)
 *     drains the registry LIFO (source/stdlib.c: exit() runs __atexit_funcs
 *     back-to-front), so a correct libc emits "321" (reverse of registration).
 *
 *   - AtexitSelftestAssert fails an assert(); assert.h expands it to
 *     __assert_fail("x == 42", file, line, func), which prints
 *     "Assertion failed: x == 42 (...)" to the unbuffered stderr and then
 *     abort()s → os.Exit(134). x is volatile so the compiler cannot fold the
 *     always-false comparison away.
 *
 * Reached FROM C across a TU boundary, so it also exercises the cross-TU
 * c2go_linkname calls into atexit()/exit()/write()/__assert_fail. */
#include <stdlib.h>   /* atexit, exit */
#include <unistd.h>   /* write */
#include <assert.h>   /* assert → __assert_fail */
#include <c2go.h>     /* c2go_extern */

static void h1(void) { write(1, "1", 1); }
static void h2(void) { write(1, "2", 1); }
static void h3(void) { write(1, "3", 1); }

/* Registration order 1,2,3 ⇒ LIFO firing order 3,2,1 ⇒ stdout "321". */
c2go_extern void AtexitSelftestLIFO(void) {
    atexit(h1);
    atexit(h2);
    atexit(h3);
    exit(0);
}

/* x == 42 is false (x is 0), so the assertion fires: __assert_fail prints the
 * diagnostic to stderr and abort()s. */
c2go_extern void AtexitSelftestAssert(void) {
    volatile int x = 0;
    assert(x == 42);
}
