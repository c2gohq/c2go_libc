package libc

import (
	"testing"
	"unsafe"
)

func TestStringCore(t *testing.T) {
	if Strcmp(csb("abc"), csb("abc")) != 0 || Strcmp(csb("abc"), csb("abd")) >= 0 {
		t.Fatal("strcmp")
	}
	if Strncmp(csb("abcX"), csb("abcY"), 3) != 0 || Strncmp(csb("ab"), csb("ac"), 2) >= 0 {
		t.Fatal("strncmp")
	}
	if Strcasecmp(csb("ABC"), csb("abc")) != 0 {
		t.Fatal("strcasecmp")
	}
	if Strnlen(csb("hello"), 10) != 5 || Strnlen(csb("hello"), 3) != 3 {
		t.Fatal("strnlen")
	}

	dst := make([]byte, 16)
	Strcpy(&dst[0], csb("hi"))
	if Strlen(&dst[0]) != 2 {
		t.Fatal("strcpy")
	}
	d2 := make([]byte, 16)
	if end := Stpcpy(&d2[0], csb("abcd")); end == nil || Strlen(&d2[0]) != 4 {
		t.Fatal("stpcpy")
	}

	hay := csb("hello world")
	if Strchr(hay, 'o') == nil || Strchr(hay, 'z') != nil {
		t.Fatal("strchr")
	}
	if Strrchr(hay, 'o') == nil {
		t.Fatal("strrchr")
	}
	if Strstr(hay, csb("world")) == nil || Strstr(hay, csb("xyz")) != nil {
		t.Fatal("strstr")
	}
	if Strspn(csb("aaab"), csb("a")) != 3 || Strcspn(csb("abc"), csb("c")) != 2 {
		t.Fatal("strspn/strcspn")
	}

	buf := []byte("abcdef\x00")
	if Memchr(unsafe.Pointer(&buf[0]), 'c', 6) == nil {
		t.Fatal("memchr")
	}
	if Memcmp(unsafe.Pointer(&buf[0]), unsafe.Pointer(&buf[0]), 6) != 0 {
		t.Fatal("memcmp")
	}

	// strtok_r splits in place using a caller-held save pointer
	src := []byte("a,b,c\x00")
	var save *byte
	if tok := StrtokR(&src[0], csb(","), &save); tok == nil || Strcmp(tok, csb("a")) != 0 {
		t.Fatal("strtok_r 1")
	}
	if tok := StrtokR(nil, csb(","), &save); tok == nil || Strcmp(tok, csb("b")) != 0 {
		t.Fatal("strtok_r 2")
	}

	// wide
	w, w2 := []int32{'h', 'i', 0}, []int32{'h', 'i', 0}
	if Wcslen(&w[0]) != 2 || Wcscmp(&w[0], &w2[0]) != 0 {
		t.Fatal("wcslen/wcscmp")
	}
}

// OLD longtail_test.go, verbatim (wave-8 tail: explicit_bzero).
func TestExplicitBzero(t *testing.T) {
	buf := []byte{0xff, 0xff, 0xff, 0xff}
	ExplicitBzero(unsafe.Pointer(&buf[0]), 4)
	for i, b := range buf {
		if b != 0 {
			t.Fatalf("byte %d not zeroed: %#x", i, b)
		}
	}
}
