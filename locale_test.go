package libc

import (
	"runtime"
	"testing"
	"unsafe"
)

// locale_test exercises the C.UTF-8-only locale (source/locale.c) through its
// c2go-bind bindings. c2go-libc has exactly one locale, so the contract under
// test is: setlocale/newlocale always yield C.UTF-8, localeconv returns the
// fixed C/POSIX lconv, and collation is by code point (strcoll==strcmp,
// wcscoll==wcscmp). The _l variants ignore their locale_t.
//
// lconv binds as an opaque `type lconv struct{}` (Go holds only the pointer;
// the C layout is authoritative), so its first field `char *decimal_point`
// is read back through unsafe at offset 0.

const (
	lcCTYPE = 0
	lcALL   = 6
)

// keepAlive pins each backing slice across a C call that reads its *byte/*int32.
func keepAlive(xs ...any) {
	for _, x := range xs {
		runtime.KeepAlive(x)
	}
}

// lcGoStr reads a NUL-terminated C string (from a *byte) into a Go string.
func lcGoStr(p *byte) string {
	if p == nil {
		return "<nil>"
	}
	var out []byte
	for i := 0; ; i++ {
		c := *(*byte)(unsafe.Add(unsafe.Pointer(p), i))
		if c == 0 {
			break
		}
		out = append(out, c)
	}
	return string(out)
}

// TestSetlocale: any valid category yields "C.UTF-8"; an out-of-range category
// is the sole failure (nil).
func TestSetlocale(t *testing.T) {
	nb, np := cbytes("") // name is ignored (only one locale)
	defer keepAlive(nb)

	for _, cat := range []int32{lcCTYPE, lcALL} {
		p := Setlocale(cat, np)
		if p == nil {
			t.Fatalf("Setlocale(%d, \"\") = nil, want C.UTF-8", cat)
		}
		if got := lcGoStr(p); got != "C.UTF-8" {
			t.Errorf("Setlocale(%d) = %q, want \"C.UTF-8\"", cat, got)
		}
	}
	if p := Setlocale(lcALL+1, np); p != nil {
		t.Errorf("Setlocale(out-of-range) = %q, want nil", lcGoStr(p))
	}
}

// TestLocaleconv: the C/POSIX lconv has decimal_point == "." (its first field).
func TestLocaleconv(t *testing.T) {
	lc := Localeconv()
	if lc == nil {
		t.Fatal("Localeconv() = nil")
	}
	// lconv.decimal_point is the first member (char *) — offset 0.
	dp := *(**byte)(unsafe.Pointer(lc))
	if got := lcGoStr(dp); got != "." {
		t.Errorf("localeconv()->decimal_point = %q, want \".\"", got)
	}
}

// TestStrcoll: C-locale collation is code-point order (== strcmp), and strcoll_l
// ignores its locale_t.
func TestStrcoll(t *testing.T) {
	ab, ap := cbytes("abc")
	bb, bp := cbytes("abc")
	cb, cp := cbytes("abd")
	defer keepAlive(ab, bb, cb)

	if r := Strcoll(ap, bp); r != 0 {
		t.Errorf("Strcoll(abc, abc) = %d, want 0", r)
	}
	if r := Strcoll(ap, cp); r >= 0 {
		t.Errorf("Strcoll(abc, abd) = %d, want <0", r)
	}
	if r := Strcoll(cp, ap); r <= 0 {
		t.Errorf("Strcoll(abd, abc) = %d, want >0", r)
	}
	// _l variant: same behavior, locale_t ignored (nil is accepted).
	if r := StrcollL(ap, cp, nil); r >= 0 {
		t.Errorf("StrcollL(abc, abd, nil) = %d, want <0", r)
	}
}

// TestStrxfrm: strxfrm returns strlen(src) and, when the buffer is large enough,
// copies src verbatim (C-locale transform is the identity).
func TestStrxfrm(t *testing.T) {
	sb, sp := cbytes("hello")
	defer keepAlive(sb)
	dst := make([]byte, 16)

	n := Strxfrm(&dst[0], sp, uint64(len(dst)))
	if n != 5 {
		t.Errorf("Strxfrm len = %d, want 5", n)
	}
	if got := lcGoStr(&dst[0]); got != "hello" {
		t.Errorf("Strxfrm dest = %q, want \"hello\"", got)
	}

	// n <= strlen: report the length but write nothing (dest untouched).
	small := make([]byte, 16)
	if n := Strxfrm(&small[0], sp, 3); n != 5 {
		t.Errorf("Strxfrm(n=3) len = %d, want 5", n)
	}
	if small[0] != 0 {
		t.Errorf("Strxfrm(n<=len) wrote to dest: %q", lcGoStr(&small[0]))
	}
	if n := StrxfrmL(&small[0], sp, uint64(len(small)), nil); n != 5 {
		t.Errorf("StrxfrmL len = %d, want 5", n)
	}
}


// TestLocaleObjectFamily: newlocale/duplocale yield a usable locale_t, freelocale
// is a safe no-op, and uselocale reports the (single) global locale.
func TestLocaleObjectFamily(t *testing.T) {
	loc := Newlocale(0, nil, nil)
	if loc == nil {
		t.Fatal("Newlocale() = nil")
	}
	dup := Duplocale(loc)
	if dup == nil {
		t.Fatal("Duplocale() = nil")
	}
	Freelocale(loc) // static C locale — must not crash

	// uselocale reports LC_GLOBAL_LOCALE == (locale_t)-1, keeping the
	// save/restore idiom sound.
	old := Uselocale(nil)
	if uintptr(unsafe.Pointer(old)) != ^uintptr(0) {
		t.Errorf("Uselocale = %#x, want LC_GLOBAL_LOCALE (all-ones)", uintptr(unsafe.Pointer(old)))
	}
}
