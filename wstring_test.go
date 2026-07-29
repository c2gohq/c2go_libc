//go:build unix

// Unix wchar_t is UTF-32; testWchar follows its target-specific signedness.
package libc

import (
	"testing"
	"unsafe"
)

// wstring_test exercises the wide string / wide memory port (source/wstring.c,
// ported verbatim from musl) through its c2go-bind bindings. On the LP64 unix
// test target wchar_t binds as a 32-bit target-specific Go type. Slices are
// held in locals so they stay alive across the calls.

// ws turns a Go string into a NUL-terminated wchar_t slice.
func ws(s string) []testWchar {
	r := []rune(s)
	out := make([]testWchar, len(r)+1) // out[len(r)] == 0 (NUL terminator)
	for i, c := range r {
		out[i] = testWchar(c)
	}
	return out
}

// pw returns the base pointer of a wchar_t slice.
func pw(a []testWchar) *testWchar { return &a[0] }

// wstr reads a NUL-terminated wide string back into a Go string.
func wstr(p *testWchar) string {
	if p == nil {
		return "<nil>"
	}
	var rs []rune
	for i := 0; ; i++ {
		c := *(*testWchar)(unsafe.Add(unsafe.Pointer(p), i*4))
		if c == 0 {
			break
		}
		rs = append(rs, rune(c))
	}
	return string(rs)
}

// TestWcsLenCmp covers wcslen/wcsnlen and the comparison family.
func TestWcsLenCmp(t *testing.T) {
	hello := ws("héllo\U00010348") // 6 runes incl. a 2-byte and a 4-byte codepoint
	if n := Wcslen(pw(hello)); n != 6 {
		t.Fatalf("Wcslen = %d, want 6", n)
	}
	if n := Wcsnlen(pw(hello), 3); n != 3 {
		t.Fatalf("Wcsnlen(,3) = %d, want 3", n)
	}
	if n := Wcsnlen(pw(hello), 100); n != 6 {
		t.Fatalf("Wcsnlen(,100) = %d, want 6", n)
	}

	abc, abc2, abd := ws("abc"), ws("abc"), ws("abd")
	if Wcscmp(pw(abc), pw(abc2)) != 0 {
		t.Fatal("Wcscmp(abc,abc) != 0")
	}
	if Wcscmp(pw(abc), pw(abd)) >= 0 {
		t.Fatal("Wcscmp(abc,abd) not < 0")
	}
	if Wcscmp(pw(abd), pw(abc)) <= 0 {
		t.Fatal("Wcscmp(abd,abc) not > 0")
	}
	// high code points must compare by value, not by any narrowing.
	hi, lo := ws("\U00010000"), ws("\U0000FFFF")
	if Wcscmp(pw(hi), pw(lo)) <= 0 {
		t.Fatal("Wcscmp(U+10000,U+FFFF) not > 0 — value comparison broken")
	}
	// wcsncmp stops at n; abc vs abd equal in first 2.
	if Wcsncmp(pw(abc), pw(abd), 2) != 0 {
		t.Fatal("Wcsncmp(abc,abd,2) != 0")
	}
	if Wcsncmp(pw(abc), pw(abd), 3) >= 0 {
		t.Fatal("Wcsncmp(abc,abd,3) not < 0")
	}
}

// TestWcsChr covers wcschr / wcsrchr, found and not-found (nil).
func TestWcsChr(t *testing.T) {
	s := ws("hello")
	base := uintptr(unsafe.Pointer(pw(s)))
	if q := Wcschr(pw(s), 'l'); q == nil || *q != 'l' ||
		uintptr(unsafe.Pointer(q)) != base+2*4 { // first 'l' at index 2
		t.Fatalf("Wcschr(hello,'l') wrong: %v", q)
	}
	if q := Wcsrchr(pw(s), 'l'); q == nil || *q != 'l' ||
		uintptr(unsafe.Pointer(q)) != base+3*4 { // last 'l' at index 3
		t.Fatalf("Wcsrchr(hello,'l') wrong: %v", q)
	}
	if q := Wcschr(pw(s), 'z'); q != nil {
		t.Fatalf("Wcschr(hello,'z') = %v, want nil", q)
	}
	if q := Wcsrchr(pw(s), 'z'); q != nil {
		t.Fatalf("Wcsrchr(hello,'z') = %v, want nil", q)
	}
	// wcschr(s,0) returns the terminating NUL position.
	if q := Wcschr(pw(s), 0); q == nil || *q != 0 ||
		uintptr(unsafe.Pointer(q)) != base+5*4 {
		t.Fatalf("Wcschr(hello,0) should hit the NUL at index 5")
	}
	// wcsrchr(s,0) also lands on the terminating NUL (a distinct path: p starts
	// at s+wcslen(s), where *p==0==c, so the scan loop never runs).
	if q := Wcsrchr(pw(s), 0); q == nil || *q != 0 ||
		uintptr(unsafe.Pointer(q)) != base+5*4 {
		t.Fatalf("Wcsrchr(hello,0) should hit the NUL at index 5")
	}
}

// TestWcsCpy covers wcscpy/wcpcpy/wcsncpy/wcpncpy (return values, NUL term,
// and the NUL padding of the bounded forms).
func TestWcsCpy(t *testing.T) {
	src := ws("hi\U00010348")
	dst := make([]testWchar, 8)
	if r := Wcscpy(pw(dst), pw(src)); unsafe.Pointer(r) != unsafe.Pointer(pw(dst)) {
		t.Fatal("Wcscpy did not return dest")
	}
	if wstr(pw(dst)) != "hi\U00010348" || dst[3] != 0 {
		t.Fatalf("Wcscpy result = %q (dst[3]=%d)", wstr(pw(dst)), dst[3])
	}

	// wcpcpy returns the terminating-NUL position.
	dst2 := make([]testWchar, 8)
	end := Wcpcpy(pw(dst2), pw(src))
	if uintptr(unsafe.Pointer(end)) != uintptr(unsafe.Pointer(pw(dst2)))+3*4 || *end != 0 {
		t.Fatalf("Wcpcpy end wrong")
	}

	// wcsncpy pads the tail with NULs up to n.
	dst3 := []testWchar{9, 9, 9, 9, 9, 9}
	Wcsncpy(pw(dst3), pw(ws("ab")), 5)
	want := []testWchar{'a', 'b', 0, 0, 0, 9} // only 5 written, index 5 untouched
	for i := range want {
		if dst3[i] != want[i] {
			t.Fatalf("Wcsncpy dst3 = %v, want %v", dst3, want)
		}
	}

	// wcpncpy returns dest + wcsnlen(src,n).
	dst4 := make([]testWchar, 6)
	e4 := Wcpncpy(pw(dst4), pw(ws("ab")), 5)
	if uintptr(unsafe.Pointer(e4)) != uintptr(unsafe.Pointer(pw(dst4)))+2*4 {
		t.Fatal("Wcpncpy end wrong")
	}
}

// TestWcsCat covers wcscat / wcsncat.
func TestWcsCat(t *testing.T) {
	buf := make([]testWchar, 16)
	copy(buf, ws("foo"))
	Wcscat(pw(buf), pw(ws("bar")))
	if wstr(pw(buf)) != "foobar" {
		t.Fatalf("Wcscat = %q, want foobar", wstr(pw(buf)))
	}

	buf2 := make([]testWchar, 16)
	copy(buf2, ws("foo"))
	Wcsncat(pw(buf2), pw(ws("barbaz")), 3)
	if wstr(pw(buf2)) != "foobar" {
		t.Fatalf("Wcsncat = %q, want foobar", wstr(pw(buf2)))
	}
}

// TestWcsStr covers wcsstr / wcswcs, found (short & long needle) and not-found.
func TestWcsStr(t *testing.T) {
	h := ws("the quick brown fox")
	base := uintptr(unsafe.Pointer(pw(h)))

	// single-element needle (wcschr fast path)
	if q := Wcsstr(pw(h), pw(ws("q"))); q == nil ||
		uintptr(unsafe.Pointer(q)) != base+4*4 {
		t.Fatal("Wcsstr single-char needle")
	}
	// multi-element needle (Two-Way path)
	if q := Wcsstr(pw(h), pw(ws("brown"))); q == nil ||
		uintptr(unsafe.Pointer(q)) != base+10*4 {
		t.Fatal("Wcsstr long needle")
	}
	// empty needle returns haystack
	if q := Wcsstr(pw(h), pw(ws(""))); uintptr(unsafe.Pointer(q)) != base {
		t.Fatal("Wcsstr empty needle should return h")
	}
	// not found
	if q := Wcsstr(pw(h), pw(ws("cat"))); q != nil {
		t.Fatalf("Wcsstr(not found) = %v, want nil", q)
	}
	// wcswcs is the alias
	if q := Wcswcs(pw(h), pw(ws("fox"))); q == nil ||
		uintptr(unsafe.Pointer(q)) != base+16*4 {
		t.Fatal("Wcswcs(fox)")
	}
}

// TestWcsSpn covers wcsspn / wcscspn / wcspbrk.
func TestWcsSpn(t *testing.T) {
	s := ws("hello, world")
	if n := Wcsspn(pw(s), pw(ws("hel"))); n != 4 { // "hell"
		t.Fatalf("Wcsspn = %d, want 4", n)
	}
	if n := Wcscspn(pw(s), pw(ws(","))); n != 5 { // "hello"
		t.Fatalf("Wcscspn = %d, want 5", n)
	}
	base := uintptr(unsafe.Pointer(pw(s)))
	if q := Wcspbrk(pw(s), pw(ws(","))); q == nil ||
		uintptr(unsafe.Pointer(q)) != base+5*4 || *q != ',' {
		t.Fatal("Wcspbrk(',')")
	}
	if q := Wcspbrk(pw(s), pw(ws("Z"))); q != nil {
		t.Fatalf("Wcspbrk(not found) = %v, want nil", q)
	}
}

// TestWcsTok covers the 3-arg reentrant wcstok tokenizer.
func TestWcsTok(t *testing.T) {
	buf := ws("  hello world  foo ")
	sep := ws(" ")
	var save *testWchar
	var got []string
	tok := Wcstok(pw(buf), pw(sep), &save)
	for tok != nil {
		got = append(got, wstr(tok))
		tok = Wcstok(nil, pw(sep), &save)
	}
	want := []string{"hello", "world", "foo"}
	if len(got) != len(want) {
		t.Fatalf("Wcstok tokens = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("Wcstok tokens = %v, want %v", got, want)
		}
	}
}

// TestWcsDup covers wcsdup (malloc-backed) then frees it.
func TestWcsDup(t *testing.T) {
	src := ws("dup\U00010348me")
	d := Wcsdup(pw(src))
	if d == nil {
		t.Fatal("Wcsdup returned nil")
	}
	if wstr(d) != "dup\U00010348me" {
		t.Fatalf("Wcsdup content = %q", wstr(d))
	}
	// duplicate must be an independent buffer, not the source.
	if unsafe.Pointer(d) == unsafe.Pointer(pw(src)) {
		t.Fatal("Wcsdup aliased the source")
	}
	Free(unsafe.Pointer(d))
}

// TestWmem covers the wide-memory family: wmemcpy / wmemmove (overlap) /
// wmemset / wmemchr / wmemcmp.
func TestWmem(t *testing.T) {
	// wmemcpy
	src := []testWchar{1, 2, 3, 4}
	dst := make([]testWchar, 4)
	if r := Wmemcpy(pw(dst), pw(src), 4); unsafe.Pointer(r) != unsafe.Pointer(pw(dst)) {
		t.Fatal("Wmemcpy did not return dest")
	}
	for i := range src {
		if dst[i] != src[i] {
			t.Fatalf("Wmemcpy dst = %v, want %v", dst, src)
		}
	}

	// wmemmove, dst>src overlap: move a[0:4] -> a[2:6]. (uintptr)d-(uintptr)s < n*4,
	// so it takes the backward-copy branch `while(n--) d[n]=s[n]`.
	a := []testWchar{1, 2, 3, 4, 5, 6, 7, 8}
	Wmemmove(&a[2], &a[0], 4)
	want := []testWchar{1, 2, 1, 2, 3, 4, 7, 8}
	for i := range want {
		if a[i] != want[i] {
			t.Fatalf("Wmemmove(dst>src overlap) = %v, want %v", a, want)
		}
	}

	// wmemmove, dst<src overlap: move a2[2:6] -> a2[0:4]. d<s so d-s wraps huge,
	// >= n*4, taking the OTHER branch `while(n--) *d++=*s++` (forward copy) — the
	// one a dst<src overlapping move needs to stay correct.
	a2 := []testWchar{1, 2, 3, 4, 5, 6}
	Wmemmove(&a2[0], &a2[2], 4)
	want2 := []testWchar{3, 4, 5, 6, 5, 6}
	for i := range want2 {
		if a2[i] != want2[i] {
			t.Fatalf("Wmemmove(dst<src overlap) = %v, want %v", a2, want2)
		}
	}

	// wmemset fills n elements with the wchar value (element-wise, not byte).
	b := make([]testWchar, 5)
	Wmemset(pw(b), 0x10348, 3)
	for i := 0; i < 3; i++ {
		if b[i] != 0x10348 {
			t.Fatalf("Wmemset b = %v", b)
		}
	}
	if b[3] != 0 || b[4] != 0 {
		t.Fatalf("Wmemset overran: %v", b)
	}

	// wmemchr finds an element within the first n; nil past it.
	c := []testWchar{'a', 'b', 'c', 'd'}
	base := uintptr(unsafe.Pointer(pw(c)))
	if q := Wmemchr(pw(c), 'c', 4); q == nil ||
		uintptr(unsafe.Pointer(q)) != base+2*4 {
		t.Fatal("Wmemchr('c')")
	}
	if q := Wmemchr(pw(c), 'c', 2); q != nil { // 'c' is beyond n=2
		t.Fatalf("Wmemchr('c',2) = %v, want nil", q)
	}

	// wmemcmp: equal, less, greater over n elements.
	x := []testWchar{1, 2, 3}
	y := []testWchar{1, 2, 3}
	z := []testWchar{1, 2, 4}
	if Wmemcmp(pw(x), pw(y), 3) != 0 {
		t.Fatal("Wmemcmp(equal) != 0")
	}
	if Wmemcmp(pw(x), pw(z), 3) >= 0 {
		t.Fatal("Wmemcmp(x<z) not < 0")
	}
	if Wmemcmp(pw(z), pw(x), 3) <= 0 {
		t.Fatal("Wmemcmp(z>x) not > 0")
	}
}
