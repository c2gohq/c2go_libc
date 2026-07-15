package libc

// Tranche A convenience additions to source/stdio.c: fileno, dprintf/vdprintf
// (unbuffered fd sink over a transient stack FILE) and asprintf/vasprintf
// (malloc'd string sink). Exercised through the c2go-bind Go bindings, reusing
// the pargs void** variadic packer from stdio_test.go.

import (
	"io"
	"os"
	"runtime"
	"testing"
	"unsafe"
)

//go:linkname cStderr github.com/c2gohq/c2go_libc.stderr
var cStderr *_c2go_FILE

//go:linkname cStdin github.com/c2gohq/c2go_libc.stdin
var cStdin *_c2go_FILE

// (cstr — NUL-terminated C string -> Go string — is shared from cstr.go.)

// TestFileno: the three standard streams report their fixed descriptors.
func TestFileno(t *testing.T) {
	cases := []struct {
		f    *_c2go_FILE
		name string
		want int32
	}{
		{cStdin, "stdin", 0},
		{cStdout, "stdout", 1},
		{cStderr, "stderr", 2},
	}
	for _, c := range cases {
		if got := Fileno(c.f); got != c.want {
			t.Errorf("Fileno(%s) = %d, want %d", c.name, got, c.want)
		}
	}
}

// TestDprintf: dprintf formats straight to a descriptor (unbuffered). Write to a
// regular temp file (no poller -> the raw write() never EAGAINs) and read back.
func TestDprintf(t *testing.T) {
	tmp, err := os.CreateTemp("", "dprintf")
	if err != nil {
		t.Fatal(err)
	}
	defer os.Remove(tmp.Name())
	defer tmp.Close()

	a := &pargs{}
	a.i(42)
	a.s("hi")
	a.f(3.5)
	fb := append([]byte("%d-%s-%.1f"), 0)
	ap, ptrs := a.packPtr()
	n := Dprintf(int32(tmp.Fd()), &fb[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(fb)

	const want = "42-hi-3.5"
	if n != int32(len(want)) {
		t.Fatalf("Dprintf returned %d, want %d", n, len(want))
	}
	if _, err := tmp.Seek(0, io.SeekStart); err != nil {
		t.Fatal(err)
	}
	data, err := io.ReadAll(tmp)
	if err != nil {
		t.Fatal(err)
	}
	if string(data) != want {
		t.Fatalf("Dprintf wrote %q, want %q", data, want)
	}
}

// TestAsprintf: asprintf mallocs an exact-size buffer, returns the length, and
// leaves *s pointing at the NUL-terminated result.
func TestAsprintf(t *testing.T) {
	a := &pargs{}
	a.i(3)
	a.i(14)
	fb := append([]byte("%d.%d!"), 0)
	ap, ptrs := a.packPtr()

	var s *byte
	n := Asprintf(&s, &fb[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(fb)

	const want = "3.14!"
	if n != int32(len(want)) {
		t.Fatalf("Asprintf returned %d, want %d", n, len(want))
	}
	if s == nil {
		t.Fatal("Asprintf left *s nil")
	}
	if got := cstr(s); got != want {
		t.Fatalf("Asprintf produced %q, want %q", got, want)
	}
	Free(unsafe.Pointer(s))
}
