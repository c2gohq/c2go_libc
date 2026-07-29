//go:build unix

// (unix-only, #649: unix wchar_t == int32; the Windows wide path (uint16)
// is covered by the wchar wine gate.)
package libc

// wstdio_printf_test exercises the wide FORMATTED output ported from musl
// (source/stdio.c: wprintf_core + vfwprintf/vswprintf and the swprintf/fwprintf/
// wprintf/vwprintf wrappers). Swprintf formats into a wchar_t (int32) buffer;
// every result is cross-checked against Go's own fmt formatting. Fwprintf writes
// to a real file (Fopen "w") and the on-disk UTF-8 bytes are compared to the
// expected string. wchar_t is int32 (UTF-32) on the darwin/linux test targets.
//
// Helpers reused from the package (NOT redefined here): pargs + its i/s/f
// builders and packPtr (stdio_test.go), wpush (wstdio_test.go), wslice + wopen
// (wstdio_fileio_test.go).

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

// swf formats cfmt (a C wide format string) with the accumulated args via
// Swprintf into a wchar_t buffer and returns the decoded Go string.
func swf(t *testing.T, cfmt string, a *pargs) string {
	t.Helper()
	wf := wslice(cfmt)
	buf := make([]testWchar, 256)
	ap, ptrs := a.packPtr()
	n := Swprintf(&buf[0], uint64(len(buf)), &wf[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(wf)
	runtime.KeepAlive(buf)
	if n < 0 {
		t.Fatalf("Swprintf(%q) = %d", cfmt, n)
	}
	rs := make([]rune, n)
	for i := int32(0); i < n; i++ {
		rs[i] = rune(buf[i])
	}
	return string(rs)
}

// TestSwprintfBasic drives one directive per case and cross-checks against Go's
// own fmt (the C conversions chosen all agree with Go for these inputs).
func TestSwprintfBasic(t *testing.T) {
	cases := []struct {
		cfmt  string
		build func(*pargs)
		want  string
	}{
		// integer
		{"%d", func(a *pargs) { a.i(42) }, fmt.Sprintf("%d", 42)},
		{"%d", func(a *pargs) { a.i(-5) }, fmt.Sprintf("%d", -5)},
		// narrow string widened via mbtowc (ASCII + multibyte)
		{"%s", func(a *pargs) { a.s("hi") }, fmt.Sprintf("%s", "hi")},
		{"%s", func(a *pargs) { a.s("café") }, fmt.Sprintf("%s", "café")},
		// wide string (wchar_t*)
		{"%ls", func(a *pargs) { wpush(a, "héllo你好") }, "héllo你好"},
		// wide char count via btowc (ASCII)
		{"%c", func(a *pargs) { a.i(int('A')) }, fmt.Sprintf("%c", 'A')},
		// float
		{"%f", func(a *pargs) { a.f(3.14) }, fmt.Sprintf("%f", 3.14)},
		{"%.2f", func(a *pargs) { a.f(3.14159) }, fmt.Sprintf("%.2f", 3.14159)},
		// width / precision / left-adjust
		{"%5d", func(a *pargs) { a.i(42) }, fmt.Sprintf("%5d", 42)},
		{"%-8s|", func(a *pargs) { a.s("hi") }, fmt.Sprintf("%-8s|", "hi")},
		{"%08.2f", func(a *pargs) { a.f(3.14159) }, fmt.Sprintf("%08.2f", 3.14159)},
	}
	for _, c := range cases {
		a := &pargs{}
		c.build(a)
		got := swf(t, c.cfmt, a)
		if got != c.want {
			t.Errorf("Swprintf(%q) = %q, want %q", c.cfmt, got, c.want)
		}
	}
}

// TestSwprintfMixed: several directives in one call keep the va cursor aligned
// and interleave literal text.
func TestSwprintfMixed(t *testing.T) {
	a := &pargs{}
	a.i(7)
	wpush(a, "世界")
	a.f(2.5)
	got := swf(t, "n=%d s=[%ls] f=%.1f", a)
	want := "n=7 s=[世界] f=2.5"
	if got != want {
		t.Errorf("Swprintf mixed = %q, want %q", got, want)
	}
}

// TestSwprintfReturnCount: swprintf returns the wide-char count (not bytes).
func TestSwprintfReturnCount(t *testing.T) {
	a := &pargs{}
	wpush(a, "你好") // 2 wchars, 6 UTF-8 bytes
	wf := wslice("[%ls]")
	buf := make([]testWchar, 256)
	ap, ptrs := a.packPtr()
	n := Swprintf(&buf[0], uint64(len(buf)), &wf[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(wf)
	runtime.KeepAlive(buf)
	if n != 4 { // '[' + 2 wide + ']'
		t.Errorf("Swprintf return = %d, want 4 (wide-char count)", n)
	}
	if got := wstr(&buf[0]); got != "[你好]" {
		t.Errorf("Swprintf buf = %q, want %q", got, "[你好]")
	}
}

// TestSwprintfTruncation: an n smaller than the output truncates and returns -1.
func TestSwprintfTruncation(t *testing.T) {
	a := &pargs{}
	a.i(123456)
	wf := wslice("%d")
	buf := make([]testWchar, 4) // room for "12" + NUL (n-1 = 3 wchars fit, output is 6)
	ap, ptrs := a.packPtr()
	n := Swprintf(&buf[0], uint64(len(buf)), &wf[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(wf)
	runtime.KeepAlive(buf)
	if n != -1 {
		t.Errorf("Swprintf truncated return = %d, want -1", n)
	}
	if got := wstr(&buf[0]); got != "123" { // n-1 = 3 wchars written, NUL-terminated
		t.Errorf("Swprintf truncated buf = %q, want %q", got, "123")
	}
}

// TestFwprintfToFile: Fwprintf to a real file; the on-disk UTF-8 bytes must
// equal the expected formatted string, and the return value is the wide-char
// count.
func TestFwprintfToFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "fw.txt")
	f := wopen(t, path, "w")

	wf := wslice("x=%d s=%ls f=%.2f\n")
	a := &pargs{}
	a.i(7)
	wpush(a, "héllo你好")
	a.f(3.14159)
	ap, ptrs := a.packPtr()
	n := Fwprintf(f, &wf[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(wf)
	if n < 0 {
		t.Fatalf("Fwprintf = %d", n)
	}
	if r := Fclose(f); r != 0 {
		t.Fatalf("Fclose = %d", r)
	}

	want := "x=7 s=héllo你好 f=3.14\n"
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != want {
		t.Errorf("file bytes = %q, want %q", got, want)
	}
	if int(n) != len([]rune(want)) {
		t.Errorf("Fwprintf n=%d, want %d (wide-char count)", n, len([]rune(want)))
	}
}
