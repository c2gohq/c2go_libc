//go:build unix

// (unix-only, #649: unix wchar_t == int32; the Windows wide path (uint16) is covered by the wchar wine gate.)
package libc

// wstdio_fileio_test exercises the wide-character FILE I/O ported from musl
// (source/stdio.c: fwide/fputwc/fgetwc/fputws/fgetws/ungetwc). Each test opens a
// real file (Fopen), so the wide chars travel through the FILE's byte buffer +
// the wctomb/mbrtowc conversions. Write-close-reopen-read mirrors the narrow
// fopen tests and avoids the write<->read transition on one handle. wchar_t is
// int32 (UTF-32) on the darwin/linux test targets; every value is cross-checked
// against Go's own UTF-8<->rune. (cbytes is shared from fopen_test.go.)

import (
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

// wopen opens path with mode and returns the FILE* (fatal on nil).
func wopen(t *testing.T, path, mode string) *FILE {
	t.Helper()
	pb, pp := cbytes(path)
	mb, mp := cbytes(mode)
	f := Fopen(pp, mp)
	runtime.KeepAlive(pb)
	runtime.KeepAlive(mb)
	if f == nil {
		t.Fatalf("Fopen(%q,%q) = nil", path, mode)
	}
	return f
}

// wslice builds a NUL-terminated wchar_t (int32) slice from s.
func wslice(s string) []int32 {
	rs := []rune(s)
	w := make([]int32, len(rs)+1)
	for i, r := range rs {
		w[i] = int32(r)
	}
	return w
}

// TestFwideOrientation: fwide sets the byte/wide orientation once and then only
// reports it (a set orientation is sticky).
func TestFwideOrientation(t *testing.T) {
	f := wopen(t, filepath.Join(t.TempDir(), "o.txt"), "w")
	defer Fclose(f)
	if m := Fwide(f, 0); m != 0 {
		t.Fatalf("fwide(f,0) on fresh FILE = %d, want 0 (unset)", m)
	}
	if m := Fwide(f, 1); m != 1 {
		t.Fatalf("fwide(f,1) = %d, want 1 (wide)", m)
	}
	if m := Fwide(f, -1); m != 1 {
		t.Fatalf("fwide(f,-1) after wide = %d, want 1 (orientation is sticky)", m)
	}
	if m := Fwide(f, 0); m != 1 {
		t.Fatalf("fwide(f,0) query = %d, want 1", m)
	}
}

// TestFputwcReadback: fputwc a mixed ASCII/multibyte sequence, close, and verify
// the file holds the exact UTF-8 bytes.
func TestFputwcReadback(t *testing.T) {
	path := filepath.Join(t.TempDir(), "put.txt")
	text := "Aé你\n"
	f := wopen(t, path, "w")
	for _, r := range text {
		if c := Fputwc(int32(r), f); c != uint32(r) {
			t.Fatalf("Fputwc(%q) = %#x, want %#x", r, c, uint32(r))
		}
	}
	if r := Fclose(f); r != 0 {
		t.Fatalf("Fclose = %d", r)
	}
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != text {
		t.Errorf("file bytes = %q, want %q", got, text)
	}
}

// TestFgetwcRoundTrip: write UTF-8 to a file, then fgetwc it back into wchars,
// ending in WEOF.
func TestFgetwcRoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "get.txt")
	text := "héllo你好"
	if err := os.WriteFile(path, []byte(text), 0644); err != nil {
		t.Fatal(err)
	}
	f := wopen(t, path, "r")
	defer Fclose(f)
	for _, r := range text {
		if c := Fgetwc(f); c != uint32(r) {
			t.Fatalf("Fgetwc = %#x, want %#x (%q)", c, uint32(r), r)
		}
	}
	if c := Fgetwc(f); c != weof {
		t.Errorf("Fgetwc at EOF = %#x, want WEOF", c)
	}
}

// TestUngetwc: fgetwc then ungetwc pushes the char back so the next fgetwc
// returns it again (covers both a multibyte and an ASCII push-back).
func TestUngetwc(t *testing.T) {
	path := filepath.Join(t.TempDir(), "unget.txt")
	if err := os.WriteFile(path, []byte("你X"), 0644); err != nil {
		t.Fatal(err)
	}
	f := wopen(t, path, "r")
	defer Fclose(f)

	c := Fgetwc(f) // 你
	if c != uint32('你') {
		t.Fatalf("Fgetwc = %#x, want %#x", c, uint32('你'))
	}
	if u := Ungetwc(c, f); u != c {
		t.Fatalf("Ungetwc(多字节) = %#x, want %#x", u, c)
	}
	if c2 := Fgetwc(f); c2 != uint32('你') {
		t.Fatalf("Fgetwc after ungetwc = %#x, want %#x", c2, uint32('你'))
	}
	if c3 := Fgetwc(f); c3 != uint32('X') {
		t.Fatalf("Fgetwc = %#x, want %#x", c3, uint32('X'))
	}
}

// TestFgetwcIllegal drives the byte-by-byte error path: a valid lead byte
// followed by a non-continuation byte makes mbrtowc report EILSEQ mid-sequence,
// so fgetwc returns WEOF, sets the error flag, and pushes the stray byte back
// via the non-locking ungetc core. Completing at all proves that core no longer
// self-deadlocks under the FILE lock.
func TestFgetwcIllegal(t *testing.T) {
	path := filepath.Join(t.TempDir(), "bad.txt")
	// 'A', then 0xE4 (a 3-byte UTF-8 lead) followed by 0x41 ('A', not a
	// continuation byte) — an illegal sequence after a partial one.
	if err := os.WriteFile(path, []byte{'A', 0xe4, 0x41}, 0644); err != nil {
		t.Fatal(err)
	}
	f := wopen(t, path, "r")
	defer Fclose(f)

	if c := Fgetwc(f); c != uint32('A') {
		t.Fatalf("Fgetwc = %#x, want 'A'", c)
	}
	*ErrnoPtr() = 0
	if c := Fgetwc(f); c != weof {
		t.Fatalf("Fgetwc on illegal seq = %#x, want WEOF", c)
	}
	if e := *ErrnoPtr(); e != errEILSEQ {
		t.Errorf("errno = %d, want EILSEQ(%d)", e, errEILSEQ)
	}
}

// TestFputwsFgetws: fputws a wide string, verify UTF-8 on disk, then fgetws it
// back (fgetws keeps the trailing newline and NUL-terminates).
func TestFputwsFgetws(t *testing.T) {
	path := filepath.Join(t.TempDir(), "ws.txt")
	text := "Wide 世界 line\n"

	w := wslice(text)
	f := wopen(t, path, "w")
	if r := Fputws(&w[0], f); r < 0 {
		t.Fatalf("Fputws = %d", r)
	}
	runtime.KeepAlive(w)
	if r := Fclose(f); r != 0 {
		t.Fatalf("Fclose = %d", r)
	}
	if got, _ := os.ReadFile(path); string(got) != text {
		t.Errorf("file = %q, want %q", got, text)
	}

	g := wopen(t, path, "r")
	defer Fclose(g)
	buf := make([]int32, 64)
	rp := Fgetws(&buf[0], int32(len(buf)), g)
	runtime.KeepAlive(buf)
	if rp == nil {
		t.Fatal("Fgetws returned nil")
	}
	want := []rune(text) // includes the trailing '\n'
	for i, r := range want {
		if buf[i] != int32(r) {
			t.Errorf("buf[%d] = %#x, want %#x (%q)", i, buf[i], r, r)
		}
	}
	if buf[len(want)] != 0 {
		t.Errorf("fgetws not NUL-terminated at %d: %#x", len(want), buf[len(want)])
	}
}
