//go:build darwin || freebsd || linux || netbsd

package dl_test

import (
	"math"
	"runtime"
	"testing"
	"unsafe"

	"github.com/c2gohq/c2go_libc/dl"
)

// sym resolves a libc/libm symbol via the default namespace, skipping the
// subtest if the host does not expose it that way (e.g. some Linux setups keep
// libm out of RTLD_DEFAULT). On darwin everything below is in libSystem.
func sym(t *testing.T, name string) uintptr {
	t.Helper()
	fn, err := dl.Dlsym(dl.RTLD_DEFAULT, name)
	if err != nil || fn == 0 {
		t.Skipf("Dlsym(RTLD_DEFAULT, %q): %v", name, err)
	}
	return fn
}

func fbits(v float64) uintptr { return uintptr(math.Float64bits(v)) }

// TestCall_Mixed pins the property plain SyscallN cannot deliver: a function
// mixing an integer and a float argument, with a float return.
//
//	double ldexp(double x, int exp) = x * 2^exp
func TestCall_Mixed(t *testing.T) {
	ldexp := sym(t, "ldexp")
	_, f := dl.Call(ldexp,
		[]dl.Piece{{Class: dl.PFloat, Val: fbits(1.5)}, {Class: dl.PInt, Val: 3}},
		0, dl.RFloat, nil, nil)
	if got := math.Float64frombits(uint64(f)); got != 12.0 {
		t.Fatalf("ldexp(1.5, 3) = %v, want 12", got)
	}
}

// TestCall_Floats: pure-float args + float return (pow, fma).
func TestCall_Floats(t *testing.T) {
	_, f := dl.Call(sym(t, "pow"),
		[]dl.Piece{{Class: dl.PFloat, Val: fbits(2)}, {Class: dl.PFloat, Val: fbits(10)}},
		0, dl.RFloat, nil, nil)
	if got := math.Float64frombits(uint64(f)); got != 1024.0 {
		t.Fatalf("pow(2,10) = %v, want 1024", got)
	}

	_, f = dl.Call(sym(t, "fma"),
		[]dl.Piece{{Class: dl.PFloat, Val: fbits(2)}, {Class: dl.PFloat, Val: fbits(3)}, {Class: dl.PFloat, Val: fbits(4)}},
		0, dl.RFloat, nil, nil)
	if got := math.Float64frombits(uint64(f)); got != 10.0 {
		t.Fatalf("fma(2,3,4) = %v, want 10", got)
	}
}

// TestCall_Pointers: pointer arg + integer return (strlen), and
// pointer+int+int -> pointer return (memchr).
func TestCall_Pointers(t *testing.T) {
	buf := []byte("hello\x00")
	base := uintptr(unsafe.Pointer(&buf[0]))

	a, _ := dl.Call(sym(t, "strlen"),
		[]dl.Piece{{Class: dl.PInt, Val: base}}, 0, dl.RInt, nil, nil)
	runtime.KeepAlive(buf)
	if a != 5 {
		t.Fatalf("strlen(\"hello\") = %d, want 5", a)
	}

	a, _ = dl.Call(sym(t, "memchr"),
		[]dl.Piece{{Class: dl.PInt, Val: base}, {Class: dl.PInt, Val: uintptr('l')}, {Class: dl.PInt, Val: 5}},
		0, dl.RInt, nil, nil)
	runtime.KeepAlive(buf)
	if want := base + 2; a != want {
		t.Fatalf("memchr(\"hello\", 'l', 5) = %#x, want %#x", a, want)
	}
}
