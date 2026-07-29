//go:build unix

// Unix wchar_t is UTF-32; testWchar follows its target-specific signedness.
package libc

// wstdio_test exercises the wide-character %ls/%lc/%S/%C directives now wired
// into the C printf core (source/stdio.c printf_core, via musl's wctomb) and the
// scanf core (vfscanf, via musl's mbrtowc). On the darwin/linux test targets
// wchar_t is UTF-32; every result is cross-checked against Go's own
// UTF-8 <-> rune conversion. These are the end-to-end checks that the batch-2
// multibyte port actually drives real printf/scanf output, not just its unit
// tests. (Windows wchar_t is uint16/UTF-16 with surrogates — deferred.)

import (
	"runtime"
	"testing"
	"unsafe"
)

// wpush builds a NUL-terminated wchar_t array from s, roots it on a
// (same escape discipline as pargs.s), and pushes the void* to its first element
// as the %ls pointer argument.
func wpush(a *pargs, s string) {
	rs := []rune(s)
	w := make([]testWchar, len(rs)+1)
	for i, r := range rs {
		w[i] = testWchar(r)
	}
	a.keep = append(a.keep, w)
	a.cells = append(a.cells, uint64(uintptr(unsafe.Pointer(&w[0]))))
}

// wcpush pushes a single wide char as a wint_t (unsigned int, 4-byte cell) for %lc.
func wcpush(a *pargs, r rune) {
	a.cells = append(a.cells, uint64(uint32(r)))
}

// TestSnprintfWideString: %ls converts a wchar_t* to its UTF-8 byte sequence.
func TestSnprintfWideString(t *testing.T) {
	for _, text := range []string{"你好世界", "Résumé café", "ascii only", ""} {
		a := &pargs{}
		wpush(a, text)
		got, n := snf(t, "%ls", a)
		if got != text {
			t.Errorf("Snprintf(%%ls, %q) = %q, want %q", text, got, text)
		}
		if int(n) != len(text) {
			t.Errorf("Snprintf(%%ls, %q) n=%d, want %d (UTF-8 byte length)", text, n, len(text))
		}
	}
}

// TestSnprintfWideChar: %lc converts one wint_t to its UTF-8 byte sequence.
func TestSnprintfWideChar(t *testing.T) {
	for _, r := range []rune{'A', '好', 'é', '世'} {
		a := &pargs{}
		wcpush(a, r)
		got, n := snf(t, "%lc", a)
		want := string(r)
		if got != want {
			t.Errorf("Snprintf(%%lc, %q) = %q, want %q", r, got, want)
		}
		if int(n) != len(want) {
			t.Errorf("Snprintf(%%lc, %q) n=%d, want %d", r, n, len(want))
		}
	}
}

// TestSnprintfWideCursor: a %ls between two %d must keep the va cursor aligned
// (the same cursor-integrity check the Phase-1 float placeholder had).
func TestSnprintfWideCursor(t *testing.T) {
	a := &pargs{}
	a.i(11)
	wpush(a, "中")
	a.i(22)
	got, _ := snf(t, "%d[%ls]%d", a)
	if want := "11[中]22"; got != want {
		t.Errorf("Snprintf cursor = %q, want %q", got, want)
	}
}

// TestSnprintfWidePrecision: %.Nls caps the OUTPUT at N BYTES, never splitting a
// multibyte char (musl's measure loop enforces l<=p-i).
func TestSnprintfWidePrecision(t *testing.T) {
	// "你好" is 6 UTF-8 bytes (3+3). A precision of 5 must emit only "你" (3
	// bytes): the second char would need bytes 4..6, exceeding 5.
	a := &pargs{}
	wpush(a, "你好")
	got, n := snf(t, "%.5ls", a)
	if want := "你"; got != want {
		t.Errorf("Snprintf(%%.5ls, 你好) = %q (n=%d), want %q", got, n, want)
	}
}

// TestSscanfWideString: %ls decodes UTF-8 input into a wchar_t array and NUL-
// terminates it, stopping at whitespace like %s.
func TestSscanfWideString(t *testing.T) {
	for _, text := range []string{"你好世界", "café", "abc"} {
		wbuf := make([]testWchar, 32)
		n := ssf(t, text, "%ls", unsafe.Pointer(&wbuf[0]))
		runtime.KeepAlive(wbuf)
		if n != 1 {
			t.Fatalf("Sscanf(%q, %%ls) = %d, want 1", text, n)
		}
		rs := []rune(text)
		for i, r := range rs {
			if wbuf[i] != testWchar(r) {
				t.Errorf("[%q] wbuf[%d] = %#x, want %#x (%q)", text, i, wbuf[i], r, r)
			}
		}
		if wbuf[len(rs)] != 0 {
			t.Errorf("[%q] wbuf not NUL-terminated at %d: %#x", text, len(rs), wbuf[len(rs)])
		}
	}
}

// TestSscanfWideStops: %ls stops at the first whitespace (scanset excludes it).
func TestSscanfWideStops(t *testing.T) {
	wbuf := make([]testWchar, 32)
	n := ssf(t, "café latte", "%ls", unsafe.Pointer(&wbuf[0]))
	runtime.KeepAlive(wbuf)
	if n != 1 {
		t.Fatalf("Sscanf(%%ls) = %d, want 1", n)
	}
	want := []rune("café")
	for i, r := range want {
		if wbuf[i] != testWchar(r) {
			t.Errorf("wbuf[%d] = %#x, want %#x", i, wbuf[i], r)
		}
	}
	if wbuf[len(want)] != 0 {
		t.Errorf("wbuf not NUL-terminated: %#x", wbuf[len(want)])
	}
}

// TestSscanfWideCharASCII: %lc reads one (single-byte) wide char, no NUL term.
func TestSscanfWideCharASCII(t *testing.T) {
	wc := ^testWchar(0)
	n := ssf(t, "Z", "%lc", unsafe.Pointer(&wc))
	if n != 1 {
		t.Fatalf("Sscanf(%%lc) = %d, want 1", n)
	}
	if wc != 'Z' {
		t.Errorf("wc = %#x, want %#x", wc, 'Z')
	}
}

// TestWideRoundTrip: printf %ls -> UTF-8 bytes -> scanf %ls -> wchar_t array,
// end-to-end through both ported halves.
func TestWideRoundTrip(t *testing.T) {
	text := "héllo你好"
	a := &pargs{}
	wpush(a, text)
	got, _ := snf(t, "%ls", a)
	if got != text {
		t.Fatalf("printf %%ls = %q, want %q", got, text)
	}
	wbuf := make([]testWchar, 32)
	n := ssf(t, got, "%ls", unsafe.Pointer(&wbuf[0]))
	runtime.KeepAlive(wbuf)
	if n != 1 {
		t.Fatalf("scanf %%ls = %d, want 1", n)
	}
	for i, r := range []rune(text) {
		if wbuf[i] != testWchar(r) {
			t.Errorf("roundtrip wbuf[%d] = %#x, want %#x", i, wbuf[i], r)
		}
	}
}
