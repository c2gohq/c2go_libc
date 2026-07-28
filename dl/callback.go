//go:build darwin || freebsd || linux || netbsd

// Reverse-callback bridge for c2go_callback(fn): the path a foreign C thread
// takes when it calls back into a c2go function.
//
// clang's __c2go_callback(fn) yields the address of a per-fn cdecl trampoline
// (emitted by c2go-bind in the generated package). The foreign library calls
// that trampoline by the native C ABI; the trampoline spills the C-ABI argument
// registers into an args region, burns the per-sig converter + the target's Go
// ABI0 entry into a cbFrame, and crosscall2's into Go. fakecgo provides
// crosscall2 under CGO_ENABLED=0 (pulled in transitively, same as cgodispatch).
//
// The hops, and which package each piece lives in:
//
//	[C lib] --cdecl--> cdecl_tramp        (generated pkg, c2go-bind .s)
//	  --crosscall2--> runtime.cgocallback --> cbentry  (THIS pkg, ABIInternal)
//	  --cbinvoke--> converter             (generated pkg, clang GoABI0)
//	  --> destFn                          (generated pkg, the user's c2go func)
//
// Only cdecl_tramp -> cbentry needs a symbol reference (the cbentry ABIInternal
// entry); like c2go_syscallX, c2go-bind emits a package-local
// `var c2go_cbentry_fn uintptr` initialized from CbentryFn(). Every other hop is
// an indirect call through a pointer the trampoline burned into the cbFrame.

package dl

import "unsafe"

// cbFrame is the per-callback frame the cdecl trampoline builds on the (C)
// stack and crosscall2 hands to cbentry. Its layout MUST match clang's
// converter (EmitC2GoCallbackConverters) and the trampoline .s:
//
//	converter @0   per-sig GoABI0 converter  (clang: c2go_cbconv_<fn>)
//	destFn    @8   target's Go ABI0 entry    (·<fn>)
//	args      @16  -> the C-ABI register spill region [F0..F7, intRegs, stack]
//	result    @24  out: result word(s); the trampoline reads result[0] back into
//	               R0/AX (int/pointer) or F0/XMM0 (float)
type cbFrame struct {
	converter uintptr
	destFn    uintptr
	args      unsafe.Pointer
	result    [4]uintptr
}

// cbentry is the universal ABIInternal landing pad. The cdecl trampoline calls
// crosscall2(c2go_cbentry_fn, &cbFrame, 0, 0); fakecgo's crosscall2 ->
// runtime.cgocallback builds a func value from c2go_cbentry_fn and calls it
// ABIInternal with the frame. cbentry then hops to the per-sig ABI0 converter
// (which clang synthesized) via the cbinvoke asm helper.
func cbentry(frame unsafe.Pointer) {
	f := (*cbFrame)(frame)
	cbinvoke(f.converter, frame)
}

// cbentryFV pins cbentry as a func value so we can take its ABIInternal entry
// (the funcval.fn word) — the entry runtime.cgocallback must be handed. Passing
// the raw ABI0 .s entry instead crashes (proven in the de-risk PoC).
var cbentryFV = cbentry

// cbentryFn caches cbentry's ABIInternal entry (funcval.fn), computed once.
var cbentryFn uintptr

func init() {
	// A func value is a pointer to a funcval{fn, ...}; funcval.fn (first word)
	// is the ABIInternal entry runtime.cgocallback expects.
	cbentryFn = **(**uintptr)(unsafe.Pointer(&cbentryFV))
}

// CbentryFn returns cbentry's ABIInternal entry PC. c2go-bind emits, once per
// generated package, `var c2go_cbentry_fn uintptr` initialized from this, and
// the cdecl trampoline loads `·c2go_cbentry_fn(SB)` as crosscall2's fn argument.
func CbentryFn() uintptr { return cbentryFn }

// cbinvoke (cb_GOARCH.s) indirectly GoABI0-calls converter(frame), bridging the
// ABIInternal cbentry to the ABI0 converter clang emitted.
func cbinvoke(converter uintptr, frame unsafe.Pointer)
