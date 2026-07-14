package libc

import "testing"

func TestStrlen(t *testing.T) {
	b := []byte("hello, c2go\x00")
	if got := Strlen(&b[0]); got != 11 {
		t.Fatalf("Strlen=%d, want 11", got)
	}
	e := []byte("\x00")
	if got := Strlen(&e[0]); got != 0 {
		t.Fatalf("Strlen(empty)=%d, want 0", got)
	}
}
