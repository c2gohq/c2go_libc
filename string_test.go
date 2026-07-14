package libc

import (
	"testing"
	"unsafe"
)

func TestStringCore(t *testing.T) {
	if Strcmp(cstr("abc"), cstr("abc")) != 0 || Strcmp(cstr("abc"), cstr("abd")) >= 0 {
		t.Fatal("strcmp")
	}
	if Strncmp(cstr("abcX"), cstr("abcY"), 3) != 0 || Strncmp(cstr("ab"), cstr("ac"), 2) >= 0 {
		t.Fatal("strncmp")
	}
	if Strcasecmp(cstr("ABC"), cstr("abc")) != 0 {
		t.Fatal("strcasecmp")
	}
	if Strnlen(cstr("hello"), 10) != 5 || Strnlen(cstr("hello"), 3) != 3 {
		t.Fatal("strnlen")
	}

	dst := make([]byte, 16)
	Strcpy(&dst[0], cstr("hi"))
	if Strlen(&dst[0]) != 2 {
		t.Fatal("strcpy")
	}
	d2 := make([]byte, 16)
	if end := Stpcpy(&d2[0], cstr("abcd")); end == nil || Strlen(&d2[0]) != 4 {
		t.Fatal("stpcpy")
	}

	hay := cstr("hello world")
	if Strchr(hay, 'o') == nil || Strchr(hay, 'z') != nil {
		t.Fatal("strchr")
	}
	if Strrchr(hay, 'o') == nil {
		t.Fatal("strrchr")
	}
	if Strstr(hay, cstr("world")) == nil || Strstr(hay, cstr("xyz")) != nil {
		t.Fatal("strstr")
	}
	if Strspn(cstr("aaab"), cstr("a")) != 3 || Strcspn(cstr("abc"), cstr("c")) != 2 {
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
	if tok := StrtokR(&src[0], cstr(","), &save); tok == nil || Strcmp(tok, cstr("a")) != 0 {
		t.Fatal("strtok_r 1")
	}
	if tok := StrtokR(nil, cstr(","), &save); tok == nil || Strcmp(tok, cstr("b")) != 0 {
		t.Fatal("strtok_r 2")
	}

	// wide
	w, w2 := []int32{'h', 'i', 0}, []int32{'h', 'i', 0}
	if Wcslen(&w[0]) != 2 || Wcscmp(&w[0], &w2[0]) != 0 {
		t.Fatal("wcslen/wcscmp")
	}
}
