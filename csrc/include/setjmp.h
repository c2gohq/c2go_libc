/* setjmp.h — non-local jumps on Go goroutine stacks (#653).
 *
 * The classic jmp_buf (callee-saved registers + a raw SP) cannot work here:
 * the goroutine stack MOVES (runtime copystack on growth and GC shrink), so a
 * saved raw SP goes stale — and the c2go backend never carries values in
 * callee-saved registers across call sites anyway (all live state sits in
 * stack slots, which copystack relocates). So the c2go jmp_buf saves only
 * {g, g.stack.hi, sp, bp(amd64), resume pc, result-slot offset}, and longjmp
 * re-anchors sp/bp by delta = (current g.stack.hi - saved g.stack.hi) before
 * jumping — the same hi-relative math copystack itself uses. Implementation:
 * sjlj.go + sjlj_<arch>.s on the Go side of the package.
 *
 * Hard rules (documented UB, cgo-parity):
 *   - longjmp must target a live setjmp frame of the SAME goroutine; a
 *     cross-goroutine longjmp panics (fail-loud, not silent stack surgery).
 *   - never longjmp across a Go frame (a C -> Go callback boundary): the
 *     abandoned Go frame's defers are not run.
 *   - standard C: non-volatile locals modified after setjmp are indeterminate
 *     on the second return (returns_twice makes clang spill accordingly).
 *   - the signal mask is not saved/restored (the c2go signal model has no
 *     per-thread mask); _setjmp/_longjmp are therefore exact aliases.
 */
#ifndef _SETJMP_H
#define _SETJMP_H

#include <c2go.h>

/* {g, stackhi, sp, bp, pc, retoff} + 2 spare slots. */
typedef unsigned long long jmp_buf[8];

int setjmp(jmp_buf) __attribute__((returns_twice))
    c2go_linkname("github.com/c2gohq/c2go_libc.setjmp", C2GO_GOABI0);
_Noreturn void longjmp(jmp_buf, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.longjmp", C2GO_GOABI0);

/* The C standard requires setjmp to be usable as a macro. */
#define setjmp setjmp

/* POSIX no-signal-mask variants (Lua uses these under LUA_USE_POSIX). */
int _setjmp(jmp_buf) __attribute__((returns_twice))
    c2go_linkname("github.com/c2gohq/c2go_libc.setjmp", C2GO_GOABI0);
_Noreturn void _longjmp(jmp_buf, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.longjmp", C2GO_GOABI0);

#endif /* _SETJMP_H */
