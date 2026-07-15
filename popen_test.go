//go:build unix

package libc

// #662 — popen/pclose over the os/exec bridge + fdopen (musl shape).

import (
	"testing"
	"unsafe"
)

func TestPopenReadAndStatus(t *testing.T) {
	f := Popen(csb("echo hi; exit 0"), csb("r"))
	if f == nil {
		t.Fatal("popen(r) failed")
	}
	buf := make([]byte, 32)
	if Fgets(&buf[0], 32, f) == nil {
		t.Fatal("fgets from popen returned NULL")
	}
	if got := cstr(&buf[0]); got != "hi\n" {
		t.Fatalf("popen output = %q, want %q", got, "hi\n")
	}
	if st := Pclose(f); st != 0 {
		t.Fatalf("pclose = %#x, want 0", st)
	}

	f = Popen(csb("exit 3"), csb("r"))
	if f == nil {
		t.Fatal("popen(exit 3) failed")
	}
	st := Pclose(f)
	if (st&0xff00)>>8 != 3 { // WEXITSTATUS
		t.Fatalf("pclose status = %#x, want WEXITSTATUS 3", st)
	}
}

func TestPopenWrite(t *testing.T) {
	f := Popen(csb("wc -c > /dev/null"), csb("w"))
	if f == nil {
		t.Fatal("popen(w) failed")
	}
	msg := []byte("hello\n")
	if n := Fwrite(unsafe.Pointer(&msg[0]), 1, uint64(len(msg)), f); n != uint64(len(msg)) {
		t.Fatalf("fwrite to popen = %d", n)
	}
	if st := Pclose(f); st != 0 {
		t.Fatalf("pclose(w) = %#x, want 0", st)
	}
	// pclose on a non-popen stream: EINVAL.
	g := Tmpfile()
	if g == nil {
		t.Fatal("tmpfile failed")
	}
	*ErrnoPtr() = 0
	if r := Pclose(g); r != -1 || *ErrnoPtr() != errEINVAL {
		t.Fatalf("pclose(non-popen) = %d errno=%d, want -1/EINVAL", r, *ErrnoPtr())
	}
	Fclose(g)
}
