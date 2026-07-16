//go:build unix

// uchar.h (C11 char16_t/char32_t) tests — #691 wiring. Unlike wchar_t, char16_t
// is UTF-16 on EVERY target, so the surrogate pair split (mbrtoc16 -3 protocol)
// and combine (c16rtomb parked-high state) get native coverage here; the
// windows-only branches these functions guard (16-bit wchar_t) reuse the same
// arithmetic. U+1F48A: UTF-8 f0 9f 92 8a, UTF-16 pair D83D DC8A.
package libc

import "testing"

const errNeg3 = ^uint64(2) // (size_t)-3: mbrtoc16 delivered the pair's low half

func TestMbrtoc32(t *testing.T) {
	pill := []byte("\xf0\x9f\x92\x8a")
	var c32 uint32

	// One-shot supplementary decode: full 32-bit scalar, 4 bytes consumed.
	st, _ := newState()
	if got := Mbrtoc32(&c32, &pill[0], 4, st); got != 4 {
		t.Fatalf("Mbrtoc32(pill) = %d, want 4", got)
	}
	if c32 != 0x1F48A {
		t.Errorf("Mbrtoc32(pill) c32 = %#x, want 0x1F48A", c32)
	}

	// Streaming resume across a 2+2 split.
	st2, raw := newState()
	if got := Mbrtoc32(&c32, &pill[0], 2, st2); got != errNeg2 {
		t.Fatalf("Mbrtoc32(pill[:2]) = %#x, want -2", got)
	}
	if raw[0] == 0 {
		t.Error("state must be non-initial after a partial decode")
	}
	if got := Mbrtoc32(&c32, rawp(pill[2:]), 2, st2); got != 2 {
		t.Fatalf("Mbrtoc32(resume) = %d, want 2 (bytes consumed this call)", got)
	}
	if c32 != 0x1F48A {
		t.Errorf("Mbrtoc32(resume) c32 = %#x, want 0x1F48A", c32)
	}

	// Illegal sequence: -1 + EILSEQ.
	st3, _ := newState()
	*ErrnoPtr() = 0
	if got := Mbrtoc32(&c32, rawp([]byte{0xFF, 0}), 1, st3); got != errNeg1 {
		t.Errorf("Mbrtoc32(0xFF) = %#x, want -1", got)
	}
	if e := *ErrnoPtr(); e != eilseq() {
		t.Errorf("Mbrtoc32(0xFF) errno = %d, want EILSEQ(%d)", e, eilseq())
	}
}

func TestMbrtoc16(t *testing.T) {
	pill := []byte("\xf0\x9f\x92\x8a")
	x := []byte("X")
	var u16 uint16

	// Supplementary: call 1 consumes all 4 bytes and delivers the HIGH unit;
	// call 2 consumes nothing, returns -3, and delivers the parked LOW unit.
	st, raw := newState()
	if got := Mbrtoc16(&u16, &pill[0], 4, st); got != 4 {
		t.Fatalf("Mbrtoc16(pill) = %d, want 4", got)
	}
	if u16 != 0xD83D {
		t.Errorf("Mbrtoc16(pill) high = %#x, want 0xD83D", u16)
	}
	if raw[0] == 0 {
		t.Error("low half must be pending in the state after the high half")
	}
	if got := Mbrtoc16(&u16, &x[0], 1, st); got != errNeg3 {
		t.Fatalf("Mbrtoc16(pending) = %#x, want -3", got)
	}
	if u16 != 0xDC8A {
		t.Errorf("Mbrtoc16(pending) low = %#x, want 0xDC8A", u16)
	}
	if raw[0] != 0 {
		t.Error("state must be initial after the low half is delivered")
	}
	// The input byte offered alongside the -3 call was NOT consumed: it decodes now.
	if got := Mbrtoc16(&u16, &x[0], 1, st); got != 1 || u16 != 'X' {
		t.Errorf("Mbrtoc16('X') = %d/%#x, want 1/'X'", got, u16)
	}

	// BMP passthrough (é U+00E9, c3 a9): no pair, no pending.
	st2, raw2 := newState()
	if got := Mbrtoc16(&u16, rawp([]byte("\xc3\xa9")), 2, st2); got != 2 || u16 != 0xE9 {
		t.Errorf("Mbrtoc16(é) = %d/%#x, want 2/0xE9", got, u16)
	}
	if raw2[0] != 0 {
		t.Error("BMP decode must leave the state initial")
	}
}

func TestC16rtomb(t *testing.T) {
	var buf [8]byte

	// BMP unit encodes directly.
	st, _ := newState()
	if got := C16rtomb(&buf[0], 0x00E9, st); got != 2 || buf[0] != 0xc3 || buf[1] != 0xa9 {
		t.Errorf("C16rtomb(0xE9) = %d % x, want 2 c3 a9", got, buf[:2])
	}

	// High surrogate parks (returns 0, nothing written); the low completes the
	// pair into the 4-byte UTF-8 of U+1F48A.
	if got := C16rtomb(&buf[0], 0xD83D, st); got != 0 {
		t.Fatalf("C16rtomb(high) = %d, want 0 (parked)", got)
	}
	if got := C16rtomb(&buf[0], 0xDC8A, st); got != 4 {
		t.Fatalf("C16rtomb(low) = %d, want 4", got)
	}
	if string(buf[:4]) != "\xf0\x9f\x92\x8a" {
		t.Errorf("C16rtomb(pair) bytes = % x, want f0 9f 92 8a", buf[:4])
	}

	// A lone low surrogate is illegal.
	st2, _ := newState()
	*ErrnoPtr() = 0
	if got := C16rtomb(&buf[0], 0xDC8A, st2); got != errNeg1 {
		t.Errorf("C16rtomb(lone low) = %#x, want -1", got)
	}
	if e := *ErrnoPtr(); e != eilseq() {
		t.Errorf("C16rtomb(lone low) errno = %d, want EILSEQ(%d)", e, eilseq())
	}

	// High followed by a non-low is illegal and clears the parked state.
	st3, raw3 := newState()
	if got := C16rtomb(&buf[0], 0xD83D, st3); got != 0 {
		t.Fatalf("C16rtomb(high) = %d, want 0", got)
	}
	*ErrnoPtr() = 0
	if got := C16rtomb(&buf[0], 0x0041, st3); got != errNeg1 {
		t.Errorf("C16rtomb(high,'A') = %#x, want -1", got)
	}
	if raw3[0] != 0 {
		t.Error("ilseq must clear the parked-high state")
	}
}

func TestC32rtomb(t *testing.T) {
	var buf [8]byte

	// ASCII / BMP / supplementary encode widths.
	cases := []struct {
		c32  uint32
		want string
	}{
		{'A', "A"},
		{0x00E9, "\xc3\xa9"},
		{0x6C34, "\xe6\xb0\xb4"},
		{0x1F48A, "\xf0\x9f\x92\x8a"},
	}
	for _, c := range cases {
		st, _ := newState()
		got := C32rtomb(&buf[0], c.c32, st)
		if got != uint64(len(c.want)) || string(buf[:len(c.want)]) != c.want {
			t.Errorf("C32rtomb(%#x) = %d % x, want %d % x", c.c32, got, buf[:4], len(c.want), []byte(c.want))
		}
	}

	// Out of Unicode range and surrogate values are illegal.
	for _, bad := range []uint32{0x110000, 0xD800, 0xDFFF} {
		st, _ := newState()
		*ErrnoPtr() = 0
		if got := C32rtomb(&buf[0], bad, st); got != errNeg1 {
			t.Errorf("C32rtomb(%#x) = %#x, want -1", bad, got)
		}
		if e := *ErrnoPtr(); e != eilseq() {
			t.Errorf("C32rtomb(%#x) errno = %d, want EILSEQ(%d)", bad, e, eilseq())
		}
	}

	// Roundtrip through the decode core.
	st, _ := newState()
	var back uint32
	n := C32rtomb(&buf[0], 0x1F48A, st)
	if got := Mbrtoc32(&back, &buf[0], n, st); got != n || back != 0x1F48A {
		t.Errorf("roundtrip U+1F48A: decode = %d/%#x, want %d/0x1F48A", got, back, n)
	}
}

// TestMbsrtowcsCountingResumed guards the #689 counting-branch RESTRUCTURE on
// unix: the counting pass (ws==NULL) must agree with the writing pass for a
// string whose first char is RESUMED from a prior partial decode (drives the
// new resume0 accumulate path). NOTE it does NOT observe the #689 bug itself —
// the miscount (a resumed supplementary must count 2 UTF-16 units) is
// windows-only and its guard constant-folds away here; behavioural coverage
// belongs to the windows wchar gate (#694).
func TestMbsrtowcsCountingResumed(t *testing.T) {
	// "a" + U+1F48A + "é", NUL-terminated.
	s := []byte("a\xf0\x9f\x92\x8a\xc3\xa9\x00")

	// Whole-string: counting == writing.
	stC, _ := newState()
	srcC := &s[0]
	nC := Mbsrtowcs(nil, &srcC, 0, stC)
	stW, _ := newState()
	srcW := &s[0]
	var w [8]int32
	nW := Mbsrtowcs(&w[0], &srcW, 8, stW)
	if nC != nW || nC != 3 {
		t.Fatalf("count %d vs write %d, want 3 == 3", nC, nW)
	}
	if w[0] != 'a' || w[1] != 0x1F48A || w[2] != 0xE9 {
		t.Errorf("written = %#x %#x %#x, want 'a' 0x1F48A 0xE9", w[0], w[1], w[2])
	}

	// Resumed: prime the state with 2 of the supplementary's 4 bytes, then hand
	// the remainder to the counting pass (drives the resume0 accumulate path).
	stR, _ := newState()
	var wc int32
	if got := Mbrtowc(&wc, &s[1], 2, stR); got != errNeg2 {
		t.Fatalf("prime Mbrtowc = %#x, want -2", got)
	}
	rest := &s[3] // 92 8a c3 a9 00
	if got := Mbsrtowcs(nil, &rest, 0, stR); got != 2 {
		t.Errorf("resumed count = %d, want 2 (U+1F48A + é)", got)
	}

	// Same priming, writing pass: resumed char decodes whole.
	stR2, _ := newState()
	if got := Mbrtowc(&wc, &s[1], 2, stR2); got != errNeg2 {
		t.Fatalf("prime Mbrtowc = %#x, want -2", got)
	}
	rest2 := &s[3]
	var w2 [4]int32
	if got := Mbsrtowcs(&w2[0], &rest2, 4, stR2); got != 2 || w2[0] != 0x1F48A || w2[1] != 0xE9 {
		t.Errorf("resumed write = %d (%#x %#x), want 2 (0x1F48A 0xE9)", got, w2[0], w2[1])
	}
}
