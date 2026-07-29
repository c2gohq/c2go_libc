//go:build unix

// (unix-only, #649: uses unix-only syscall API.)
package libc

import (
	"math"
	"os"
	"runtime"
	"testing"
	"unsafe"

	"golang.org/x/sys/unix"
)

// escapeSink / escape force a destination's referent onto the heap. The scanf
// pack cells below launder destination addresses through uintptr — invisible
// to the GC — so the destination MUST be heap-allocated. `new`/`make` do NOT
// guarantee that: a non-escaping destination is stack-allocated, its
// uintptr-laundered address goes stale when the goroutine stack is copied
// mid-call (first deep call on a fresh goroutine), and the callee then stores
// through the abandoned old stack — the result silently "disappears" (#603:
// TestSscanf* failed exactly this way whenever the growth phase lined up).

func TestSscanfSmoke(t *testing.T) {
	n := new(int32)
	got := ssf(t, "42", "%d", unsafe.Pointer(n))
	if got != 1 {
		t.Fatalf("Sscanf returned %d, want 1", got)
	}
	if *n != 42 {
		t.Fatalf("scanned %d, want 42", *n)
	}
}

// TestSscanfInt covers the integer conversions (%d/%i/%o/%u/%x) incl. sign,
// auto-base (%i), and the return (match) count.
func TestSscanfInt(t *testing.T) {
	cases := []struct {
		input, fmt string
		want       int64
	}{
		{"42", "%d", 42},
		{"-7", "%d", -7},
		{"+15", "%d", 15},
		{"   99", "%d", 99}, // leading whitespace skipped
		{"0x1A", "%x", 0x1A},
		{"ff", "%x", 0xff},
		{"755", "%o", 0o755},
		{"4294967290", "%u", 4294967290},
		{"0x2a", "%i", 42},   // %i auto: hex
		{"052", "%i", 42},    // %i auto: octal
		{"42", "%i", 42},     // %i auto: decimal
		{"-0x10", "%i", -16}, // %i signed hex
	}
	for _, c := range cases {
		d := new(int64)
		n := ssf(t, c.input, c.fmt, unsafe.Pointer(d))
		if n != 1 {
			t.Errorf("Sscanf(%q,%q) returned %d, want 1", c.input, c.fmt, n)
			continue
		}
		// %d/%i/%o/%u/%x without a length modifier store an int (32-bit);
		// use SIZE_l (%ld etc.) below for 64-bit. Here read low 32 bits.
		got := int64(int32(*d))
		if c.fmt == "%u" {
			got = int64(uint32(*d))
		}
		if got != c.want {
			t.Errorf("Sscanf(%q,%q) = %d, want %d", c.input, c.fmt, got, c.want)
		}
	}
}

// TestSscanfLong covers length modifiers (%ld/%lld 64-bit, %hd short, %hhd char).
func TestSscanfLong(t *testing.T) {
	{
		d := new(int64)
		if n := ssf(t, "9223372036854775807", "%lld", unsafe.Pointer(d)); n != 1 || *d != 9223372036854775807 {
			t.Errorf("%%lld = %d (n=%d), want 9223372036854775807", *d, n)
		}
	}
	{
		d := new(int64)
		if n := ssf(t, "1234567890", "%ld", unsafe.Pointer(d)); n != 1 || *d != 1234567890 {
			t.Errorf("%%ld = %d (n=%d), want 1234567890", *d, n)
		}
	}
	{
		d := new(int16)
		if n := ssf(t, "-30000", "%hd", unsafe.Pointer(d)); n != 1 || *d != -30000 {
			t.Errorf("%%hd = %d (n=%d), want -30000", *d, n)
		}
	}
	{
		d := new(int8)
		if n := ssf(t, "-42", "%hhd", unsafe.Pointer(d)); n != 1 || *d != -42 {
			t.Errorf("%%hhd = %d (n=%d), want -42", *d, n)
		}
	}
}

// TestSscanfString covers %s, %c (with width), and the return count.
func TestSscanfString(t *testing.T) {
	{
		b, p := strDest(32)
		if n := ssf(t, "  hello world", "%s", p); n != 1 {
			t.Fatalf("%%s n=%d, want 1", n)
		}
		if got := cstr(&b[0]); got != "hello" {
			t.Errorf("%%s = %q, want %q", got, "hello")
		}
	}
	{
		b, p := strDest(8)
		if n := ssf(t, "abcdef", "%3c", p); n != 1 {
			t.Fatalf("%%3c n=%d, want 1", n)
		}
		// %c does NOT NUL-terminate; compare the first 3 bytes.
		if got := string(b[:3]); got != "abc" {
			t.Errorf("%%3c = %q, want %q", got, "abc")
		}
	}
	{
		b, p := strDest(8)
		if n := ssf(t, "X", "%c", p); n != 1 || b[0] != 'X' {
			t.Errorf("%%c = %q (n=%d), want X", b[0], n)
		}
	}
}

// TestSscanfScanset covers %[...] positive/negative sets and ranges.
func TestSscanfScanset(t *testing.T) {
	cases := []struct {
		input, fmt, want string
	}{
		{"hello123", "%[a-z]", "hello"},
		{"hello123", "%[^0-9]", "hello"},
		{"12345abc", "%[0-9]", "12345"},
		{"aXbXc def", "%[^ ]", "aXbXc"},
		{"]end", "%[]]", "]"}, // leading ] is a literal member
	}
	for _, c := range cases {
		b, p := strDest(32)
		n := ssf(t, c.input, c.fmt, p)
		if n != 1 {
			t.Errorf("Sscanf(%q,%q) n=%d, want 1", c.input, c.fmt, n)
			continue
		}
		if got := cstr(&b[0]); got != c.want {
			t.Errorf("Sscanf(%q,%q) = %q, want %q", c.input, c.fmt, got, c.want)
		}
	}
}

// TestSscanfMulti covers multiple args, %n (chars consumed), %* suppression,
// literal matching, and partial-match return counts.
func TestSscanfMulti(t *testing.T) {
	{
		a, b := new(int32), new(int32)
		bs, sp := strDest(16)
		n := ssf(t, "1 2 three", "%d %d %s", unsafe.Pointer(a), unsafe.Pointer(b), sp)
		if n != 3 || *a != 1 || *b != 2 || cstr(&bs[0]) != "three" {
			t.Errorf("multi = (%d,%d,%q) n=%d, want (1,2,three) n=3", *a, *b, cstr(&bs[0]), n)
		}
	}
	{
		// %*d suppresses the first field; only the second is stored.
		d := new(int32)
		n := ssf(t, "111 222", "%*d %d", unsafe.Pointer(d))
		if n != 1 || *d != 222 {
			t.Errorf("suppress = %d n=%d, want 222 n=1", *d, n)
		}
	}
	{
		// %n reports the byte count consumed so far (does not add to matches).
		d, cnt := new(int32), new(int32)
		n := ssf(t, "  4567xyz", "%d%n", unsafe.Pointer(d), unsafe.Pointer(cnt))
		if n != 1 || *d != 4567 || *cnt != 6 {
			t.Errorf("%%n = d=%d cnt=%d n=%d, want d=4567 cnt=6 n=1", *d, *cnt, n)
		}
	}
	{
		// literal chars in the format must match input.
		a, b := new(int32), new(int32)
		n := ssf(t, "12:34", "%d:%d", unsafe.Pointer(a), unsafe.Pointer(b))
		if n != 2 || *a != 12 || *b != 34 {
			t.Errorf("literal = (%d,%d) n=%d, want (12,34) n=2", *a, *b, n)
		}
	}
	{
		// mismatch on the literal stops scanning after the first field.
		a, b := new(int32), new(int32)
		n := ssf(t, "12-34", "%d:%d", unsafe.Pointer(a), unsafe.Pointer(b))
		if n != 1 || *a != 12 {
			t.Errorf("partial = (%d,%d) n=%d, want (12,_) n=1", *a, *b, n)
		}
	}
}

// TestSscanfEmpty: no conversions match on empty/mismatched input → EOF (-1)
// when nothing was matched.
func TestSscanfEmpty(t *testing.T) {
	d := new(int32)
	if n := ssf(t, "", "%d", unsafe.Pointer(d)); n != -1 {
		t.Errorf("empty input n=%d, want -1 (EOF)", n)
	}
	if n := ssf(t, "abc", "%d", unsafe.Pointer(d)); n != 0 {
		t.Errorf("non-numeric n=%d, want 0 (match fail)", n)
	}
}

// TestSscanfFloat covers the float conversions (#589, via __floatscan): %lf
// (double dest), %f (float dest), exponents, hex floats, and sign.
func TestSscanfFloat(t *testing.T) {
	dcases := []struct {
		in   string
		want float64
	}{
		{"3.14159", 3.14159},
		{"-2.5", -2.5},
		{"+0.0", 0.0},
		{"1e10", 1e10},
		{"1.5e-3", 1.5e-3},
		{"123456.789", 123456.789},
		{"0x1.8p3", 12.0}, // hex float: 1.5 * 2^3
		{"0x1p-4", 0.0625},
		{"  6.022e23", 6.022e23}, // leading whitespace
		{".5", 0.5},
		{"100", 100.0},
	}
	for _, c := range dcases {
		d := new(float64)
		n := ssf(t, c.in, "%lf", unsafe.Pointer(d))
		if n != 1 || *d != c.want {
			t.Errorf("Sscanf(%q,%%lf) = %v (n=%d), want %v", c.in, *d, n, c.want)
		}
	}
	// %f stores a 32-bit float.
	{
		f := new(float32)
		n := ssf(t, "2.5", "%f", unsafe.Pointer(f))
		if n != 1 || *f != 2.5 {
			t.Errorf("%%f = %v (n=%d), want 2.5", *f, n)
		}
	}
	// %e / %g are the same conversion as %f for input.
	{
		d := new(float64)
		if n := ssf(t, "1.25E2", "%le", unsafe.Pointer(d)); n != 1 || *d != 125.0 {
			t.Errorf("%%le = %v (n=%d), want 125", *d, n)
		}
	}
	// float mixed with int conversions.
	{
		d, e := new(int32), new(float64)
		n := ssf(t, "42 3.5", "%d %lf", unsafe.Pointer(d), unsafe.Pointer(e))
		if n != 2 || *d != 42 || *e != 3.5 {
			t.Errorf("multi = (%d, %v) n=%d, want (42, 3.5) n=2", *d, *e, n)
		}
	}
}

// TestSscanfFloatSpecial covers inf / infinity / nan parsing.
func TestSscanfFloatSpecial(t *testing.T) {
	{
		d := new(float64)
		if n := ssf(t, "inf", "%lf", unsafe.Pointer(d)); n != 1 || !math.IsInf(*d, 1) {
			t.Errorf("inf = %v (n=%d), want +Inf", *d, n)
		}
	}
	{
		d := new(float64)
		if n := ssf(t, "-INFINITY", "%lf", unsafe.Pointer(d)); n != 1 || !math.IsInf(*d, -1) {
			t.Errorf("-INFINITY = %v (n=%d), want -Inf", *d, n)
		}
	}
	{
		d := new(float64)
		if n := ssf(t, "NaN", "%lf", unsafe.Pointer(d)); n != 1 || !math.IsNaN(*d) {
			t.Errorf("NaN = %v (n=%d), want NaN", *d, n)
		}
	}
}

// TestScanfFromFd drives the real fd read path that sscanf's string cookie
// bypasses: Scanf → vfscanf → __uflow → stdin.read == __stdio_read → read(0).
// fd 0 is redirected to a temp file holding the input, then restored. Mirrors
// TestPrintfToFd's dup2 save/restore.
func TestScanfFromFd(t *testing.T) {
	tmp, err := os.CreateTemp("", "c2go_scanf_*.txt")
	if err != nil {
		t.Fatal(err)
	}
	defer os.Remove(tmp.Name())
	if _, err := tmp.WriteString("314 hello 2718\n"); err != nil {
		t.Fatal(err)
	}
	tmp.Close()

	rf, err := os.Open(tmp.Name())
	if err != nil {
		t.Fatal(err)
	}
	defer rf.Close()

	saved, err := unix.Dup(0)
	if err != nil {
		t.Fatal(err)
	}
	if err := unix.Dup2(int(rf.Fd()), 0); err != nil {
		t.Fatal(err)
	}

	a, c := new(int32), new(int32)
	bs, sp := strDest(16)
	// pack: three destination pointers.
	cells := []uint64{
		uint64(uintptr(escape(unsafe.Pointer(a)))),
		uint64(uintptr(escape(sp))),
		uint64(uintptr(escape(unsafe.Pointer(c)))),
	}
	ptrs := make([]unsafe.Pointer, len(cells)+1)
	for i := range cells {
		ptrs[i] = unsafe.Pointer(&cells[i])
	}
	fb := append([]byte("%d %s %d"), 0)
	n := Scanf(&fb[0], unsafe.Pointer(&ptrs[0]))
	runtime.KeepAlive(cells)
	runtime.KeepAlive(ptrs)

	// restore fd 0
	unix.Dup2(saved, 0)
	unix.Close(saved)

	if n != 3 || *a != 314 || cstr(&bs[0]) != "hello" || *c != 2718 {
		t.Fatalf("Scanf-from-fd = (%d,%q,%d) n=%d, want (314,hello,2718) n=3",
			*a, cstr(&bs[0]), *c, n)
	}
}
