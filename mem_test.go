package libc

import (
	"testing"
	"unsafe"
)

// Exercises the byte-blob primitives directly so dead-code elimination cannot
// hide them: these are the routing targets of the c2go-memcpy-typing pass and
// must link + run (built verbatim from musl src/string/{mem,wmem}*.c).
func TestMem(t *testing.T) {
	src := []byte("hello world")
	dst := make([]byte, len(src))
	Memcpy(unsafe.Pointer(&dst[0]), unsafe.Pointer(&src[0]), uint64(len(src)))
	if string(dst) != "hello world" {
		t.Fatalf("memcpy: %q", dst)
	}

	buf := []byte{9, 9, 9, 9, 9, 9, 9, 9}
	Memset(unsafe.Pointer(&buf[0]), 'A', 5)
	if string(buf[:5]) != "AAAAA" || buf[5] != 9 {
		t.Fatalf("memset: %v", buf)
	}

	Bzero(unsafe.Pointer(&buf[0]), 3)
	if buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 'A' {
		t.Fatalf("bzero: %v", buf)
	}

	// overlapping move: "abcdef", copy [0:4] onto [2:] -> "ababcd"
	m := []byte("abcdef")
	Memmove(unsafe.Pointer(&m[2]), unsafe.Pointer(&m[0]), 4)
	if string(m) != "ababcd" {
		t.Fatalf("memmove: %q", m)
	}

	// wide fill + copy
	ws := []testWchar{9, 9, 9, 9}
	Wmemset(&ws[0], 'Z', 3)
	if ws[0] != 'Z' || ws[1] != 'Z' || ws[2] != 'Z' || ws[3] != 9 {
		t.Fatalf("wmemset: %v", ws)
	}
	wd := make([]testWchar, 3)
	Wmemcpy(&wd[0], &ws[0], 3)
	if wd[0] != 'Z' || wd[1] != 'Z' || wd[2] != 'Z' {
		t.Fatalf("wmemcpy: %v", wd)
	}
}
