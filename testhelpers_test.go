// testhelpers_test.go — shared, PORTABLE test helpers (#649). Collected here
// (no build tag) so per-OS tags on the files that used to define them cannot
// orphan their other users: cbytes/csb (C strings), escape (heap-escape for
// scanf destinations, #603), strDest, and the pargs variadic-pack builder
// (#588 sentinel contract) used by the printf/scanf/wide-stdio suites.

package libc

import (
	"math"
	"runtime"
	"testing"
	"unsafe"
)

// pargs accumulates variadic arguments as the width the C side's va_arg reads
// (promoted: 4-byte int cell for d/i/u/x/X/o/c/hh/h, 8-byte for l/ll/z/p/s/f),
// keeping any backing storage (C strings) alive until the call completes.
type pargs struct {
	cells []uint64
	keep  []any
}

func (a *pargs) i(v int)       { a.cells = append(a.cells, uint64(uint32(int32(v)))) } // int
func (a *pargs) u(v uint32)    { a.cells = append(a.cells, uint64(v)) }                // unsigned int
func (a *pargs) l(v int64)     { a.cells = append(a.cells, uint64(v)) }                // long / long long
func (a *pargs) z(v uint64)    { a.cells = append(a.cells, v) }                        // size_t
func (a *pargs) f(v float64)   { a.cells = append(a.cells, math.Float64bits(v)) }      // double
func (a *pargs) p(v uintptr)   { a.cells = append(a.cells, uint64(v)) }                // void*
func (a *pargs) s(str string) {
	b := append([]byte(str), 0)
	a.keep = append(a.keep, b)
	a.cells = append(a.cells, uint64(uintptr(unsafe.Pointer(&b[0]))))
}

// packPtr returns the void** cursor for the accumulated cells (nil when empty).
//
// #588 ABI contract: the pack is allocated with ONE TRAILING SENTINEL slot.
// The callee's va cursor ends one-past-the-end of the consumed arguments and
// lives in a GC-marked va_list slot across safepoints; without the sentinel,
// Go's precise GC resolves that past-the-end address as a reference to the
// NEXT heap object ("marked free object" fatal). The extra slot keeps the
// cursor's final value inside the pack allocation.
func (a *pargs) packPtr() (unsafe.Pointer, []unsafe.Pointer) {
	if len(a.cells) == 0 {
		return nil, nil
	}
	ptrs := make([]unsafe.Pointer, len(a.cells)+1) // +1: #588 sentinel
	for i := range a.cells {
		ptrs[i] = unsafe.Pointer(&a.cells[i])
	}
	return unsafe.Pointer(&ptrs[0]), ptrs
}

// cbytes makes a NUL-terminated C string. The returned slice must be kept alive
// (runtime.KeepAlive) across the C call that reads the *byte.
func cbytes(s string) ([]byte, *byte) {
	b := append([]byte(s), 0)
	return b, &b[0]
}

// Storing the pointer in a global makes it escape, which is all we need; the
// Go heap is non-moving.
//
//go:noinline
func escape(p unsafe.Pointer) unsafe.Pointer {
	escapeSink = p
	escapeSink = nil
	return p
}

var escapeSink unsafe.Pointer

// strDest returns a fixed heap buffer + its void* for a %s/%c/%[ destination.
func strDest(n int) ([]byte, unsafe.Pointer) {
	b := make([]byte, n)
	return b, unsafe.Pointer(&b[0])
}

func csb(s string) *byte { b := append([]byte(s), 0); return &b[0] }

// The C `stdout` global holds &__stdout_FILE; linkname a Go handle to it so the
// FILE* can be passed to Fwrite/Fflush from Go.
//
//go:linkname cStdout github.com/c2gohq/c2go_libc.stdout
var cStdout *_c2go_FILE

// snf formats via Snprintf into a 256-byte buffer and returns (string, retval).
func snf(t *testing.T, format string, a *pargs) (string, int32) {
	t.Helper()
	buf := make([]byte, 256)
	fb := append([]byte(format), 0)
	ap, ptrs := a.packPtr()
	n := Snprintf(&buf[0], uint64(len(buf)), &fb[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(fb)
	runtime.KeepAlive(buf)
	if n < 0 {
		t.Fatalf("Snprintf(%q) returned %d", format, n)
	}
	got := buf[:n]
	// The C side NUL-terminates at buf[min(n,255)]; for these short cases n<255.
	return string(got), n
}

// ssf drives Sscanf. Each variadic argument to C scanf is a POINTER to the
// destination; the c2go void** pack therefore holds one cell per destination
// whose value IS that pointer (va_arg(ap, void*) reads *(void**)argptrs[i]).
// Destinations are forced onto the heap via escape() — see its comment (#603).
func ssf(t *testing.T, input, format string, dests ...unsafe.Pointer) int32 {
	t.Helper()
	ib := append([]byte(input), 0)
	fb := append([]byte(format), 0)
	cells := make([]uint64, len(dests))
	for i, d := range dests {
		cells[i] = uint64(uintptr(escape(d)))
	}
	ptrs := make([]unsafe.Pointer, len(dests)+1) // +1: #588 past-end sentinel
	for i := range cells {
		ptrs[i] = unsafe.Pointer(&cells[i])
	}
	var ap unsafe.Pointer
	if len(ptrs) > 0 {
		ap = unsafe.Pointer(&ptrs[0])
	}
	return Sscanf(&ib[0], &fb[0], ap)
}
