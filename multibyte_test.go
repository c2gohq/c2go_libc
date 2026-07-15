//go:build unix

// (unix-only, #649: unix wchar_t == int32; the Windows wide path (uint16) is covered by the wchar wine gate.)
package libc

import (
	"runtime"
	"testing"
	"unicode/utf8"
	"unsafe"
)

// The multibyte port (source/multibyte.c) is a faithful C copy of musl. These
// tests cross-check the decode/encode ARITHMETIC against Go's unicode/utf8 and
// assert directly the boundary semantics utf8 does NOT model: the incomplete
// (size_t)-2 vs illegal (size_t)-1+EILSEQ distinction, embedded-NUL == 0, and
// the streaming mbstate_t accumulator. wchar_t binds as int32 on the LP64 test
// targets (signed on Darwin/x86_64, though AArch64-Linux widens it to uint32).
//
// The Go binding renders mbstate_t as the 0-byte opaque `__mbstate_t`, so a
// live [2]uint32 backs each state and its address is cast to *__mbstate_t (the
// C side only ever touches the first word). Passing the pointer to the bodyless
// binding forces the backing to escape to the heap → address stable across any
// stack growth.

const weof = uint32(0xffffffff) // WEOF
const errNeg1 = ^uint64(0)      // (size_t)-1  (illegal)
const errNeg2 = ^uint64(1)      // (size_t)-2  (incomplete)

// eilseq is the host's native EILSEQ (bits/errno.h): the port sets it on an
// illegal sequence. Kept per-GOOS so the test is honest on every target.
func eilseq() int32 {
	switch runtime.GOOS {
	case "darwin":
		return 92
	case "windows":
		return 42
	default: // linux (musl generic)
		return 84
	}
}

func rawp(b []byte) *byte { return &b[0] }

// newState returns a fresh (zeroed) mbstate plus its backing word so a test can
// inspect the raw accumulator.
func newState() (*__mbstate_t, *[2]uint32) {
	buf := new([2]uint32)
	return (*__mbstate_t)(unsafe.Pointer(buf)), buf
}

// TestMbrtowc covers the single-character decode arithmetic and the return
// contract: N-byte success, 0 for embedded NUL, -2 for an incomplete prefix,
// -1+EILSEQ for illegal.
func TestMbrtowc(t *testing.T) {
	cases := []struct {
		s  string
		cp int32
		n  uint64
	}{
		{"A", 'A', 1},
		{"£", 0x00a3, 2},     // £  U+00A3
		{"€", 0x20ac, 3},     // €  U+20AC
		{"\U00010348", 0x10348, 4}, // 𐍈 U+10348
	}
	for _, c := range cases {
		// cross-check the reference decode against Go's utf8 / []rune.
		r, size := utf8.DecodeRuneInString(c.s)
		if int32(r) != c.cp || uint64(size) != c.n || []rune(c.s)[0] != r {
			t.Fatalf("bad test vector %q: utf8 says (r=%#x,size=%d)", c.s, r, size)
		}
		st, _ := newState()
		b := append([]byte(c.s), 0)
		var wc int32 = -1
		got := Mbrtowc(&wc, &b[0], uint64(len(c.s)), st)
		if got != c.n {
			t.Errorf("Mbrtowc(%q) consumed %d, want %d", c.s, got, c.n)
		}
		if wc != c.cp {
			t.Errorf("Mbrtowc(%q) wc = %#x, want %#x", c.s, wc, c.cp)
		}
	}

	// Embedded NUL: returns 0 (not 1) and writes 0.
	{
		st, _ := newState()
		b := []byte{0, 0}
		var wc int32 = -1
		if got := Mbrtowc(&wc, &b[0], 1, st); got != 0 {
			t.Errorf("Mbrtowc(\"\\0\") = %d, want 0", got)
		}
		if wc != 0 {
			t.Errorf("Mbrtowc(\"\\0\") wc = %d, want 0", wc)
		}
	}

	// Incomplete: only the 1st byte of "€" (0xE2). Returns -2, leaves wc
	// untouched, and drives the state non-initial.
	{
		st, raw := newState()
		var wc int32 = -1
		got := Mbrtowc(&wc, rawp([]byte{0xE2, 0}), 1, st)
		if got != errNeg2 {
			t.Errorf("Mbrtowc(incomplete) = %#x, want -2 (%#x)", got, errNeg2)
		}
		if wc != -1 {
			t.Errorf("Mbrtowc(incomplete) wrote wc = %#x, must leave it untouched", wc)
		}
		if raw[0] == 0 || Mbsinit(st) != 0 {
			t.Errorf("state must be non-initial after a partial decode (raw=%#x, Mbsinit=%d)", raw[0], Mbsinit(st))
		}
	}

	// Illegal sequences: each returns -1 and sets errno=EILSEQ.
	illegal := []struct {
		name string
		b    []byte
	}{
		{"overlong C0 80", []byte{0xC0, 0x80, 0}},
		{"surrogate ED A0 80", []byte{0xED, 0xA0, 0x80, 0}},
		{"beyond-10FFFF F5", []byte{0xF5, 0x80, 0x80, 0x80, 0}},
		{"lone continuation 80", []byte{0x80, 0}},
	}
	for _, il := range illegal {
		st, _ := newState()
		*ErrnoPtr() = 0
		var wc int32 = -1
		got := Mbrtowc(&wc, &il.b[0], uint64(len(il.b)-1), st)
		if got != errNeg1 {
			t.Errorf("Mbrtowc(%s) = %#x, want -1 (%#x)", il.name, got, errNeg1)
		}
		if e := *ErrnoPtr(); e != eilseq() {
			t.Errorf("Mbrtowc(%s) errno = %d, want EILSEQ(%d)", il.name, e, eilseq())
		}
	}
}

// TestWcrtomb covers the single-character encode: byte round-trip vs utf8, and
// the surrogate -> -1+EILSEQ rejection.
func TestWcrtomb(t *testing.T) {
	for _, cp := range []int32{0x00a3, 0x20ac, 0x10348} {
		st, _ := newState()
		var buf [8]byte
		n := Wcrtomb(&buf[0], cp, st)

		var eb [4]byte
		en := utf8.EncodeRune(eb[:], rune(cp))
		if n != uint64(en) {
			t.Errorf("Wcrtomb(%#x) = %d bytes, want %d", cp, n, en)
			continue
		}
		if string(buf[:n]) != string(eb[:en]) {
			t.Errorf("Wcrtomb(%#x) = % x, want % x", cp, buf[:n], eb[:en])
		}
	}

	// A surrogate has no UTF-8 encoding: -1 + EILSEQ.
	{
		st, _ := newState()
		*ErrnoPtr() = 0
		var buf [8]byte
		if n := Wcrtomb(&buf[0], 0xD800, st); n != errNeg1 {
			t.Errorf("Wcrtomb(surrogate) = %#x, want -1 (%#x)", n, errNeg1)
		}
		if e := *ErrnoPtr(); e != eilseq() {
			t.Errorf("Wcrtomb(surrogate) errno = %d, want EILSEQ(%d)", e, eilseq())
		}
	}
}

// TestMultibyte covers the whole-string façades (Mbstowcs/Wcstombs), the
// stateless Mbtowc, Mbsinit(nil), and the single-byte Btowc/Wctob.
func TestMultibyte(t *testing.T) {
	// Mbtowc (no state) decodes ASCII and a 3-byte char.
	{
		var wc int32 = -1
		if n := Mbtowc(&wc, rawp([]byte{'A', 0}), 1); n != 1 || wc != 'A' {
			t.Errorf("Mbtowc(\"A\") = (%d, %#x), want (1, 0x41)", n, wc)
		}
		wc = -1
		if n := Mbtowc(&wc, rawp([]byte{0xE2, 0x82, 0xAC, 0}), 3); n != 3 || wc != 0x20ac {
			t.Errorf("Mbtowc(\"€\") = (%d, %#x), want (3, 0x20ac)", n, wc)
		}
	}

	// Mbstowcs / Wcstombs full round-trip of a mixed ASCII+multibyte string.
	s := "AbΩ€\U00010348z" // A b Ω € 𐍈 z  = 12 bytes, 6 runes
	runes := []rune(s)
	{
		sb := append([]byte(s), 0)
		ws := make([]int32, len(runes)+1)
		got := Mbstowcs(&ws[0], &sb[0], uint64(len(runes)+1))
		if got != uint64(len(runes)) {
			t.Fatalf("Mbstowcs = %d, want %d", got, len(runes))
		}
		for i, r := range runes {
			if ws[i] != int32(r) {
				t.Errorf("Mbstowcs[%d] = %#x, want %#x", i, ws[i], r)
			}
		}
		if ws[len(runes)] != 0 {
			t.Errorf("Mbstowcs did not NUL-terminate")
		}
	}
	{
		wsrc := make([]int32, len(runes)+1)
		for i, r := range runes {
			wsrc[i] = int32(r)
		}
		outb := make([]byte, 64)
		got := Wcstombs(&outb[0], &wsrc[0], uint64(len(outb)))
		if got != uint64(len(s)) {
			t.Fatalf("Wcstombs = %d, want %d", got, len(s))
		}
		if string(outb[:got]) != s {
			t.Errorf("Wcstombs = %q, want %q", string(outb[:got]), s)
		}
	}

	// Mbsinit(nil) is the initial state (true).
	if Mbsinit(nil) == 0 {
		t.Errorf("Mbsinit(nil) = false, want true")
	}

	// Btowc / Wctob single-byte conversions.
	if got := Btowc('A'); got != 'A' {
		t.Errorf("Btowc('A') = %#x, want 0x41", got)
	}
	if got := Btowc(0x80); got != weof {
		t.Errorf("Btowc(0x80) = %#x, want WEOF", got)
	}
	if got := Btowc(-1); got != weof { // EOF -> WEOF
		t.Errorf("Btowc(EOF) = %#x, want WEOF", got)
	}
	if got := Wctob('A'); got != 'A' {
		t.Errorf("Wctob('A') = %d, want 65", got)
	}
	if got := Wctob(0x100); got != -1 { // multibyte -> EOF
		t.Errorf("Wctob(0x100) = %d, want EOF(-1)", got)
	}
}
