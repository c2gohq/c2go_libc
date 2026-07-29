//go:build unix

// (unix-only, #649: unix wchar_t == int32; the Windows wide path (uint16)
// is covered by the wchar wine gate.)
package libc

// wstdio_scanf_test exercises the wide FORMATTED input ported from musl
// (source/stdio.c: vfwscanf + the swscanf/fwscanf/wscanf/vwscanf/vswscanf
// wrappers). Swscanf parses a wide source string; Fwscanf reads a real file
// (UTF-8 on disk, decoded to wchar_t by the FILE byte buffer). Every scanned
// value is cross-checked against Go's own parsing. wchar_t is int32 (UTF-32) on
// the darwin/linux test targets.
//
// Helpers reused from the package (NOT redefined): wslice + wopen
// (wstdio_fileio_test.go), cbytes (fopen_test.go), escape (scanf_test.go).

import (
	"math"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"testing"
	"unsafe"
)

// wssf drives Swscanf: `input` and `format` are Go strings converted to wide
// (int32) NUL-terminated strings via wslice; each destination is a POINTER
// packed into a void** cell (its value IS that pointer, so va_arg(ap,void*)
// yields it). Destinations are forced onto the heap via escape() (see #603).
func wssf(t *testing.T, input, format string, dests ...unsafe.Pointer) int32 {
	t.Helper()
	iw := wslice(input)
	fw := wslice(format)
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
	r := Swscanf(&iw[0], &fw[0], ap)
	runtime.KeepAlive(iw)
	runtime.KeepAlive(fw)
	runtime.KeepAlive(cells)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(dests)
	return r
}

// wbufStr decodes a NUL-terminated wchar_t buffer to a Go string.
func wbufStr(b []testWchar) string {
	rs := make([]rune, 0, len(b))
	for _, w := range b {
		if w == 0 {
			break
		}
		rs = append(rs, rune(w))
	}
	return string(rs)
}

// TestSwscanfInt: %d into an int.
func TestSwscanfInt(t *testing.T) {
	var got int32
	n := wssf(t, "  42xyz", "%d", unsafe.Pointer(&got))
	if n != 1 || got != 42 {
		t.Fatalf("Swscanf(%%d) = %d, got=%d; want n=1 got=42", n, got)
	}
	var neg int32
	n = wssf(t, "-7", "%d", unsafe.Pointer(&neg))
	if n != 1 || neg != -7 {
		t.Fatalf("Swscanf(%%d,-7) = %d, got=%d; want n=1 got=-7", n, neg)
	}
}

// TestSwscanfWideString: %ls decodes the wide input into a wchar_t array,
// stopping at whitespace, cross-checked against Go's []rune.
func TestSwscanfWideString(t *testing.T) {
	for _, text := range []string{"héllo你好", "ascii", "café"} {
		buf := make([]testWchar, 64)
		n := wssf(t, text+" tail", "%ls", unsafe.Pointer(&buf[0]))
		if n != 1 {
			t.Fatalf("Swscanf(%%ls, %q) = %d, want 1", text, n)
		}
		if got := wbufStr(buf); got != text {
			t.Errorf("Swscanf(%%ls, %q) = %q, want %q (Go []rune=%v)", text, got, text, []rune(text))
		}
	}
}

// TestSwscanfWideChar: %lc reads exactly one wide char (no NUL term).
func TestSwscanfWideChar(t *testing.T) {
	for _, text := range []string{"好bar", "Axyz", "é!"} {
		buf := []testWchar{0, 0}
		n := wssf(t, text, "%lc", unsafe.Pointer(&buf[0]))
		want := []rune(text)[0]
		if n != 1 || buf[0] != testWchar(want) {
			t.Fatalf("Swscanf(%%lc, %q) = %d, got=%#x; want n=1 got=%#x", text, n, buf[0], want)
		}
	}
}

// TestSwscanfFloat: %f into a float (SIZE_def == float in the C store).
func TestSwscanfFloat(t *testing.T) {
	for _, in := range []string{"3.14", "-0.5", "42", "1e3"} {
		var got float32
		n := wssf(t, in, "%f", unsafe.Pointer(&got))
		want64, _ := strconv.ParseFloat(in, 32)
		want := float32(want64)
		if n != 1 || math.Abs(float64(got-want)) > 1e-6*(1+math.Abs(float64(want))) {
			t.Fatalf("Swscanf(%%f, %q) = %d, got=%v; want n=1 got=%v", in, n, got, want)
		}
	}
}

// TestSwscanfSet: a %[...] scanset (narrow char destination via wctomb).
func TestSwscanfSet(t *testing.T) {
	buf := make([]byte, 32)
	n := wssf(t, "12345abc", "%[0-9]", unsafe.Pointer(&buf[0]))
	got := string(buf[:cLen(buf)])
	if n != 1 || got != "12345" {
		t.Fatalf("Swscanf(%%[0-9]) = %d, got=%q; want n=1 got=%q", n, got, "12345")
	}
	// inverted set: stop at 'x'
	buf2 := make([]byte, 32)
	n = wssf(t, "hello xworld", "%[^x]", unsafe.Pointer(&buf2[0]))
	got2 := string(buf2[:cLen(buf2)])
	if n != 1 || got2 != "hello " {
		t.Fatalf("Swscanf(%%[^x]) = %d, got=%q; want n=1 got=%q", n, got2, "hello ")
	}
}

// cLen returns the length of the NUL-terminated C string in b.
func cLen(b []byte) int {
	for i, c := range b {
		if c == 0 {
			return i
		}
	}
	return len(b)
}

// TestSwscanfMulti: a multi-directive line returns the match count and fills
// every destination.
func TestSwscanfMulti(t *testing.T) {
	var d int32
	name := make([]testWchar, 64)
	var f float32
	n := wssf(t, "123 héllo 4.5 trailing", "%d %ls %f",
		unsafe.Pointer(&d), unsafe.Pointer(&name[0]), unsafe.Pointer(&f))
	if n != 3 {
		t.Fatalf("Swscanf(multi) matched %d, want 3", n)
	}
	if d != 123 {
		t.Errorf("multi %%d = %d, want 123", d)
	}
	if got := wbufStr(name); got != "héllo" {
		t.Errorf("multi %%ls = %q, want %q", got, "héllo")
	}
	if math.Abs(float64(f-4.5)) > 1e-6 {
		t.Errorf("multi %%f = %v, want 4.5", f)
	}
}

// TestSwscanfPartial: a mismatch stops scanning and the match count reflects
// only the directives that succeeded.
func TestSwscanfPartial(t *testing.T) {
	var a, b int32
	n := wssf(t, "5 xyz", "%d %d", unsafe.Pointer(&a), unsafe.Pointer(&b))
	if n != 1 || a != 5 {
		t.Fatalf("Swscanf(partial) = %d, a=%d; want n=1 a=5", n, a)
	}
}

// TestVswscanf exercises the vswscanf code path. The V-variant takes a real,
// arch-specific va_list (*__va_list_tag on amd64, unsafe.Pointer on arm64) that
// Go can't portably construct, so it is driven through Swscanf — whose entire
// body is `vswscanf(s, fmt, ap)`, i.e. this covers the wstring_read cookie FILE
// and the vswscanf entry the same way musl's swscanf does.
func TestVswscanf(t *testing.T) {
	var a, b int32
	n := wssf(t, "77 88", "%d %d", unsafe.Pointer(&a), unsafe.Pointer(&b))
	if n != 2 || a != 77 || b != 88 {
		t.Fatalf("vswscanf(via Swscanf) = %d, a=%d b=%d; want n=2 a=77 b=88", n, a, b)
	}
}

// TestFwscanfFile scans %d and %ls from a real UTF-8 file.
func TestFwscanfFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "in.txt")
	if err := os.WriteFile(path, []byte("2026 世界程序"), 0644); err != nil {
		t.Fatal(err)
	}
	f := wopen(t, path, "r")
	defer Fclose(f)

	fw := wslice("%d %ls")
	var year int32
	word := make([]testWchar, 64)
	cells := []uint64{
		uint64(uintptr(escape(unsafe.Pointer(&year)))),
		uint64(uintptr(escape(unsafe.Pointer(&word[0])))),
	}
	ptrs := []unsafe.Pointer{unsafe.Pointer(&cells[0]), unsafe.Pointer(&cells[1]), nil}
	n := Fwscanf(f, &fw[0], unsafe.Pointer(&ptrs[0]))
	runtime.KeepAlive(fw)
	runtime.KeepAlive(cells)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(word)
	if n != 2 {
		t.Fatalf("Fwscanf = %d, want 2", n)
	}
	if year != 2026 {
		t.Errorf("Fwscanf %%d = %d, want 2026", year)
	}
	if got := wbufStr(word); got != "世界程序" {
		t.Errorf("Fwscanf %%ls = %q, want %q", got, "世界程序")
	}
}
