package libc

// #653 — setjmp/longjmp on Go goroutine stacks (include/setjmp.h has the
// design contract). The saved state is deliberately tiny: the c2go backend
// keeps every value live across a call site in a stack slot (verified under
// register pressure on both arches), and copystack relocates stack slots — so
// the only machine state a non-local jump must repair is SP (and amd64's BP
// frame base), both by the same hi-relative delta copystack itself applies.

import "unsafe"

// sjEnv mirrors the C jmp_buf layout. Every field is a plain uintptr so the
// GC never interprets the saved stack addresses as live pointers (the buffer
// itself lives in unmanaged C memory).
type sjEnv struct {
	g       uintptr // owning goroutine, identity check only
	stackhi uintptr // g.stack.hi at setjmp time — the delta anchor
	sp      uintptr // caller SP to restore (pre-delta)
	bp      uintptr // amd64 frame base (0 on arm64)
	pc      uintptr // resume pc: the instruction after CALL setjmp
	retoff  uintptr // (address of setjmp's GoABI0 result slot) - sp
}

// setjmp snapshots {g, g.stack.hi, sp, bp, pc, retoff} into env and returns 0
// (sjlj_<arch>.s). NOSPLIT and frameless: the snapshot is a single-instant
// view — no stack move can happen mid-save.
func setjmp(env unsafe.Pointer) int32

// sjG / sjStackLo / sjStackHi read the current g and its stack bounds
// (sjlj_<arch>.s). stack{lo,hi} being g's first field is a mirrored runtime
// detail — verified fail-closed by the init check below.
func sjG() uintptr
func sjStackLo() uintptr
func sjStackHi() uintptr

// sjResume writes val into the setjmp result slot and resumes at pc with the
// given sp/bp. Never returns. NOSPLIT: no stack move between the caller's
// final arithmetic and the jump.
func sjResume(usp, ubp, upc, uret uintptr, val int32)

// Runtime invariants this design leans on (mirrored, not hooked — record for
// future Go-version audits): stack GROWTH happens only synchronously on the
// owning goroutine (no safepoint between longjmpGo's delta computation and
// sjResume's jump: the call into NOSPLIT asm has no stack check), and GC
// shrinkstack skips async-preempted goroutines (isShrinkStackSafe), so no
// move can interleave. The stack{lo,hi}-at-g+0 layout is checked fail-closed
// by init below.
//
// longjmp is the C-callable ABI0 entry (linkname target in include/setjmp.h):
// a tail-jump in sjlj_<arch>.s to longjmpGo — cross-package asm references
// only see ABI0, and an asm reference is also what makes the compiler emit
// the ABI0 wrapper for the Go body.
func longjmp(env unsafe.Pointer, val int32)

// longjmpGo does the actual jump. Its own Go frame is abandoned by sjResume;
// it holds no defers or locks, so that abandonment is safe — the same is NOT
// true of arbitrary Go frames, hence the "never longjmp across a Go callback"
// rule.
func longjmpGo(env unsafe.Pointer, val int32) {
	e := (*sjEnv)(env)
	if e.g != sjG() {
		panic("c2go longjmp: jmp_buf was saved on a different goroutine (cross-goroutine longjmp is undefined)")
	}
	delta := sjStackHi() - e.stackhi
	if val == 0 {
		val = 1 // C guarantee: setjmp's second return is never 0
	}
	sp := e.sp + delta
	sjResume(sp, e.bp+delta, e.pc, sp+e.retoff, val)
	panic("unreachable")
}

// init anchors the mirrored g layout fail-closed: if a Go release ever moves
// stack{lo,hi} off the front of runtime.g, this fires at startup instead of
// corrupting a stack later. A local's address must sit inside [lo, hi) and
// the bounds must look like a real stack.
func init() {
	var probe int
	p := uintptr(unsafe.Pointer(&probe))
	lo, hi := sjStackLo(), sjStackHi()
	if !(lo < p && p < hi) || hi-lo < 256 || hi-lo > 1<<30 {
		panic("c2go setjmp: runtime.g stack-bounds mirror is wrong for this Go toolchain")
	}
}
