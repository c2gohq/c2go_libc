//go:build unix

// misc_test.go — the misc long tail (#700). TestBasenameDirname and
// TestA64lRoundTrip are OLD batch675_test.go's parts, verbatim; TestUname is a
// minimal wiring check for the Go-bridge shape (field content is OS-truth,
// asserted non-empty and NUL-terminated within the musl 65-byte field).
package libc

import (
	"testing"
	"unsafe"
)

func TestBasenameDirname(t *testing.T) {
	cases := []struct{ in, base, dir string }{
		{"/usr/lib", "lib", "/usr"},
		{"/usr/", "usr", "/"},
		{"usr", "usr", "."},
		{"/", "/", "/"},
		{"///", "/", "/"},
		{"", ".", "."},
	}
	for _, c := range cases {
		b := append([]byte(c.in), 0)
		if got := cstr(Basename(&b[0])); got != c.base {
			t.Errorf("basename(%q) = %q, want %q", c.in, got, c.base)
		}
		d := append([]byte(c.in), 0)
		if got := cstr(Dirname(&d[0])); got != c.dir {
			t.Errorf("dirname(%q) = %q, want %q", c.in, got, c.dir)
		}
	}
}

func TestA64lRoundTrip(t *testing.T) {
	for _, v := range []int64{0, 1, 63, 64, 12345, 0x7fffffff} {
		s := L64a(v)
		if got := A64l(s); got != v {
			t.Errorf("a64l(l64a(%d)) = %d", v, got)
		}
	}
}

func TestUname(t *testing.T) {
	// struct utsname = six 65-byte fields (sys/utsname.h, uniform musl shape).
	var uts [6 * 65]byte
	if rc := Uname((*utsname)(unsafe.Pointer(&uts[0]))); rc != 0 {
		t.Fatalf("uname = %d, want 0", rc)
	}
	sysname := cstr(&uts[0])
	nodename := cstr(&uts[65])
	release := cstr(&uts[130])
	if sysname == "" || release == "" {
		t.Fatalf("uname fields empty: sysname=%q release=%q", sysname, release)
	}
	if len(sysname) > 64 || len(nodename) > 64 {
		t.Fatalf("uname field overruns the 65-byte musl slot")
	}
}
