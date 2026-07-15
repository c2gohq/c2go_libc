//go:build unix

// (unix-only, #649: uses unix-only syscall API.)
package libc

// TestStdio exercises the C-implemented printf family (source/stdio.c, ported
// from musl) through the c2go-bind Go bindings. Snprintf/Sprintf/Printf are
// variadic c2go_extern functions, so their trailing argument pack is the void**
// tagged-argument array (va_arg(ap,T) == *(T*)(*ap++)); a Go caller assembles it
// exactly as io_go_test.go does for open(): one entry per vararg, each pointing
// at the arg's storage. Integer/string conversions are validated against golden
// strings; the float case is a Phase-1 placeholder ("<f>") but MUST still keep
// the va cursor aligned so the args after it format correctly.

import (
	"math"
	"os"
	"runtime"
	"syscall"
	"testing"
	"unsafe"
)




func TestSnprintfIntString(t *testing.T) {
	cases := []struct {
		format string
		build  func(*pargs)
		want   string
	}{
		// signed / unsigned decimal, incl. negatives and zero
		{"%d", func(a *pargs) { a.i(42) }, "42"},
		{"%d", func(a *pargs) { a.i(-5) }, "-5"},
		{"%d", func(a *pargs) { a.i(0) }, "0"},
		{"%i", func(a *pargs) { a.i(-123) }, "-123"},
		{"%u", func(a *pargs) { a.u(4000000000) }, "4000000000"},
		// hex / octal, incl. alt form
		{"%x", func(a *pargs) { a.u(255) }, "ff"},
		{"%X", func(a *pargs) { a.u(255) }, "FF"},
		{"%#x", func(a *pargs) { a.u(255) }, "0xff"},
		{"%o", func(a *pargs) { a.u(8) }, "10"},
		// char / string
		{"%c", func(a *pargs) { a.i('A') }, "A"},
		{"%s", func(a *pargs) { a.s("hi") }, "hi"},
		{"%.3s", func(a *pargs) { a.s("hello") }, "hel"},
		{"%10s", func(a *pargs) { a.s("hi") }, "        hi"},
		{"%-10s", func(a *pargs) { a.s("hi") }, "hi        "},
		// pointer (musl %p == %#016x on 64-bit): value 0x1234
		{"%p", func(a *pargs) { a.p(0x1234) }, "0x0000000000001234"},
		// literal percent
		{"a%%b", func(a *pargs) {}, "a%b"},
		// width / precision / flag combos
		{"%05d", func(a *pargs) { a.i(42) }, "00042"},
		{"%+d", func(a *pargs) { a.i(42) }, "+42"},
		{"% d", func(a *pargs) { a.i(42) }, " 42"},
		{"%-5d|", func(a *pargs) { a.i(42) }, "42   |"},
		// length modifiers
		{"%ld", func(a *pargs) { a.l(10000000000) }, "10000000000"},
		{"%lld", func(a *pargs) { a.l(-10000000000) }, "-10000000000"},
		{"%zu", func(a *pargs) { a.z(12345678901234) }, "12345678901234"},
		{"%hhd", func(a *pargs) { a.i(200) }, "-56"}, // (signed char)200 == -56
		// mixed literal + conversions
		{"[%d,%s,%x]", func(a *pargs) { a.i(7); a.s("go"); a.u(0xabc) }, "[7,go,abc]"},
	}
	for idx, c := range cases {
		a := &pargs{}
		c.build(a)
		got, n := snf(t, c.format, a)
		if got != c.want {
			t.Errorf("[%d] Snprintf(%q) = %q (n=%d, cells=%#x), want %q", idx, c.format, got, n, a.cells, c.want)
		}
		if int(n) != len(c.want) {
			t.Errorf("Snprintf(%q) retval = %d, want %d", c.format, n, len(c.want))
		}
	}
}

// TestSnprintfFloatCursor: %f between two %d must format the double correctly
// AND keep the va cursor aligned so the trailing %d (3) still reads right.
func TestSnprintfFloatCursor(t *testing.T) {
	a := &pargs{}
	a.i(1)
	a.f(2.5)
	a.i(3)
	got, _ := snf(t, "%d|%f|%d", a)
	if got != "1|2.500000|3" {
		t.Fatalf("float-cursor: got %q, want %q", got, "1|2.500000|3")
	}
}

// TestSnprintfFloat: fmt_fp golden cases (musl rounding semantics; verified
// against the host printf sweep in zz_fpref_test.go — %.0a of 3.0 etc. follow
// musl's round-to-nearest-even where macOS truncates).
func TestSnprintfFloat(t *testing.T) {
	cases := []struct {
		format string
		v      float64
		want   string
	}{
		{"%f", 2.5, "2.500000"},
		{"%f", math.Copysign(0, -1), "-0.000000"}, // Go's -0.0 literal folds to +0
		{"%.0f", 0.5, "0"},   // tie -> even
		{"%.0f", 1.5, "2"},   // tie -> even
		{"%.3f", 1.0 / 3.0, "0.333"},
		{"%e", 12345.678, "1.234568e+04"},
		{"%.0e", 9.9999999, "1e+01"},
		{"%g", 0.0001, "0.0001"},
		{"%g", 0.00001, "1e-05"},
		{"%.17g", 0.1, "0.10000000000000001"},
		{"%a", 1.0, "0x1p+0"},
		{"%a", 0.5, "0x1p-1"},
		{"%.1a", 2.5, "0x1.4p+1"},
		{"%.0a", 3.0, "0x2p+1"}, // musl: 1.5 rounds to even (2); macOS truncates
		{"%A", 255.5, "0X1.FFP+7"},
		{"%f", math.Inf(1), "inf"},
		{"%+f", math.Inf(-1), "-inf"},
		{"%F", math.Inf(1), "INF"},
		{"%f", math.NaN(), "nan"},
		{"%+f", math.NaN(), "+nan"}, // musl/glibc apply the sign flag to nan
		{"%10.2f", 3.14159, "      3.14"},
		{"%-10.2f", 3.14159, "3.14      "},
		{"%010.2f", 3.14159, "0000003.14"},
		{"%#.0f", 2.0, "2."},
		{"%e", 1e300, "1.000000e+300"},
		{"%.2e", 4.9406564584124654e-324, "4.94e-324"}, // subnormal min
	}
	for _, c := range cases {
		a := &pargs{}
		a.f(c.v)
		got, _ := snf(t, c.format, a)
		if got != c.want {
			t.Errorf("Snprintf(%q, %v) = %q, want %q", c.format, c.v, got, c.want)
		}
	}
}

// snfDeep recurses to `depth` (consuming goroutine stack) and only then calls
// snprintf, so its own deep C call chain may trigger a copystack mid-call.
//
//go:noinline
func snfDeep(depth int) string {
	if depth == 0 {
		buf := make([]byte, 32)
		fb := append([]byte("%d"), 0)
		cell := uint64(42)
		ptrs := []unsafe.Pointer{unsafe.Pointer(&cell), nil} // +nil: #588 sentinel
		n := Snprintf(&buf[0], uint64(len(buf)), &fb[0], unsafe.Pointer(&ptrs[0]))
		runtime.KeepAlive(ptrs)
		runtime.KeepAlive(fb)
		s := string(buf[:max(0, int(n))])
		runtime.KeepAlive(buf)
		return s
	}
	var pad [256]byte
	pad[0] = byte(depth)
	r := snfDeep(depth - 1)
	runtime.KeepAlive(pad)
	return r
}

// TestSnprintfDeepStack guards the copystack fix: the transient sink FILE/cookie/
// buffer live on the non-moving C heap (not the stack), so a stack growth firing
// mid-format cannot dangle their pointers. Calling snprintf from many depths
// exercises growths at various points; every call must still format "42".
func TestSnprintfDeepStack(t *testing.T) {
	for d := 0; d < 400; d++ {
		if got := snfDeep(d); got != "42" {
			t.Fatalf("depth=%d: snprintf produced %q, want \"42\" (copystack regression)", d, got)
		}
	}
}

// TestSnprintfTruncation: musl returns the would-be length; the buffer is
// truncated to n-1 chars + NUL.
func TestSnprintfTruncation(t *testing.T) {
	buf := make([]byte, 3) // room for 2 chars + NUL
	fb := append([]byte("%d"), 0)
	a := &pargs{}
	a.i(12345)
	ap, ptrs := a.packPtr()
	n := Snprintf(&buf[0], 3, &fb[0], ap)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(a)
	if n != 5 {
		t.Fatalf("Snprintf truncation retval = %d, want 5", n)
	}
	if string(buf[:2]) != "12" || buf[2] != 0 {
		t.Fatalf("Snprintf truncation buffer = %q (nul=%d), want \"12\\0\"", buf[:2], buf[2])
	}
}

// TestSprintf exercises the unbounded string sink (vsprintf -> vsnprintf INT_MAX).
func TestSprintf(t *testing.T) {
	buf := make([]byte, 64)
	fb := append([]byte("%d-%s-%x"), 0)
	a := &pargs{}
	a.i(7)
	a.s("x")
	a.u(0xff)
	ap, ptrs := a.packPtr()
	n := Sprintf(&buf[0], &fb[0], ap)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(a)
	if n < 0 {
		t.Fatalf("Sprintf returned %d", n)
	}
	if got := string(buf[:n]); got != "7-x-ff" {
		t.Fatalf("Sprintf = %q, want %q", got, "7-x-ff")
	}
}

// TestPrintfToFd drives the real fd-write path: printf_core -> out -> __fwritex ->
// __stdout_write -> __stdio_write -> write(1). fd 1 is redirected to a temp file,
// then Fflush(nil) (== fflush(NULL)) flushes stdout+stderr; the bytes are read
// back and compared.
func TestPrintfToFd(t *testing.T) {
	Fflush(nil) // flush any buffered stdout to the real fd 1 before redirecting

	tmp, err := os.CreateTemp("", "c2go_stdio_*.txt")
	if err != nil {
		t.Fatal(err)
	}
	defer os.Remove(tmp.Name())

	saved, err := syscall.Dup(1)
	if err != nil {
		t.Fatal(err)
	}
	if err := syscall.Dup2(int(tmp.Fd()), 1); err != nil {
		t.Fatal(err)
	}

	fb := append([]byte("hello %d %s\n"), 0)
	a := &pargs{}
	a.i(42)
	a.s("world")
	ap, ptrs := a.packPtr()
	ret := Printf(&fb[0], ap)
	Fflush(nil)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(a)
	runtime.KeepAlive(fb)

	// restore fd 1
	syscall.Dup2(saved, 1)
	syscall.Close(saved)

	want := "hello 42 world\n"
	if int(ret) != len(want) {
		t.Errorf("Printf retval = %d, want %d", ret, len(want))
	}
	data, err := os.ReadFile(tmp.Name())
	if err != nil {
		t.Fatal(err)
	}
	if string(data) != want {
		t.Fatalf("printf-to-fd wrote %q, want %q", data, want)
	}
}

// TestFwriteStdout confirms Fwrite(ptr,1,n,stdout) returns n and the bytes reach
// fd 1, using the linkname'd stdout FILE handle.
func TestFwriteStdout(t *testing.T) {
	if cStdout == nil {
		t.Fatal("cStdout (linkname to C stdout) is nil")
	}
	Fflush(cStdout)

	tmp, err := os.CreateTemp("", "c2go_fwrite_*.txt")
	if err != nil {
		t.Fatal(err)
	}
	defer os.Remove(tmp.Name())

	saved, _ := syscall.Dup(1)
	syscall.Dup2(int(tmp.Fd()), 1)

	msg := []byte("abcde")
	n := Fwrite(unsafe.Pointer(&msg[0]), 1, uint64(len(msg)), cStdout)
	Fflush(cStdout)
	runtime.KeepAlive(msg)

	syscall.Dup2(saved, 1)
	syscall.Close(saved)

	if n != uint64(len(msg)) {
		t.Errorf("Fwrite returned %d, want %d", n, len(msg))
	}
	data, _ := os.ReadFile(tmp.Name())
	if string(data) != "abcde" {
		t.Fatalf("Fwrite wrote %q, want %q", data, "abcde")
	}
}
