//go:build unix

package libc

// Tranche C1: mkstemp (source/stdio.c, cross-platform) and realpath
// (source/fsops_posix.c over the path/filepath bridge, Unix-only), through the
// c2go-bind bindings.

import (
	"os"
	"path/filepath"
	"syscall"
	"testing"
	"unsafe"
)

func TestMkstemp(t *testing.T) {
	// Template must be a mutable buffer ending in XXXXXX; mkstemp fills it in place.
	tmpl := append([]byte(filepath.Join(os.TempDir(), "c2goXXXXXX")), 0)
	fd := Mkstemp(&tmpl[0])
	if fd < 0 {
		t.Fatalf("Mkstemp returned %d", fd)
	}
	path := cstr(&tmpl[0])
	defer os.Remove(path)
	defer func() { _ = syscall.Close(int(fd)) }()

	// The X's must have been replaced (name changed) and the file must exist 0600.
	if filepath.Base(path) == "c2goXXXXXX" {
		t.Fatalf("Mkstemp did not fill the template: %q", path)
	}
	fi, err := os.Stat(path)
	if err != nil {
		t.Fatalf("Mkstemp file missing: %v", err)
	}
	if perm := fi.Mode().Perm(); perm != 0600 {
		t.Errorf("Mkstemp perm = %o, want 600", perm)
	}

	// EINVAL: a template without the XXXXXX suffix.
	bad := append([]byte("/tmp/noXsuffix"), 0)
	if r := Mkstemp(&bad[0]); r != -1 {
		t.Errorf("Mkstemp(bad template) = %d, want -1", r)
	}
}

func TestCreat(t *testing.T) {
	path := filepath.Join(t.TempDir(), "creatfile")

	fd := Creat(csb(path), 0600)
	if fd < 0 {
		t.Fatalf("Creat returned %d", fd)
	}
	if _, err := syscall.Write(int(fd), []byte("hi")); err != nil {
		t.Errorf("write after Creat: %v", err)
	}
	_ = syscall.Close(int(fd))

	if got, err := os.ReadFile(path); err != nil || string(got) != "hi" {
		t.Fatalf("Creat file = %q, %v; want hi", got, err)
	}
	if fi, _ := os.Stat(path); fi.Mode().Perm() != 0600 {
		t.Errorf("Creat perm = %o, want 600", fi.Mode().Perm())
	}

	// creat on an existing path truncates it (O_TRUNC).
	fd2 := Creat(csb(path), 0600)
	if fd2 < 0 {
		t.Fatalf("Creat(existing) returned %d", fd2)
	}
	_ = syscall.Close(int(fd2))
	if fi, _ := os.Stat(path); fi.Size() != 0 {
		t.Errorf("Creat did not truncate: size %d", fi.Size())
	}
}

func TestRealpath(t *testing.T) {
	dir := t.TempDir()
	real := filepath.Join(dir, "file")
	if err := os.WriteFile(real, []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	// A path with a "." component, to prove canonicalisation.
	messy := filepath.Join(dir, ".", "file")

	want, err := filepath.EvalSymlinks(messy) // the oracle
	if err != nil {
		t.Fatal(err)
	}

	// Caller-supplied buffer.
	buf := make([]byte, 4096)
	pathPtr := csb(messy)
	r := Realpath(pathPtr, &buf[0])
	if r == nil {
		t.Fatal("Realpath returned NULL for an existing path")
	}
	if got := cstr(&buf[0]); got != want {
		t.Fatalf("Realpath = %q, want %q", got, want)
	}
	if uintptr(unsafe.Pointer(r)) != uintptr(unsafe.Pointer(&buf[0])) {
		t.Error("Realpath did not return the caller's buffer")
	}

	// NULL resolved_path -> malloc'd result.
	r2 := Realpath(csb(messy), nil)
	if r2 == nil {
		t.Fatal("Realpath(_, NULL) returned NULL")
	}
	if got := cstr(r2); got != want {
		t.Fatalf("Realpath(_, NULL) = %q, want %q", got, want)
	}
	Free(unsafe.Pointer(r2))

	// Nonexistent path -> NULL.
	if Realpath(csb(filepath.Join(dir, "nope")), &buf[0]) != nil {
		t.Error("Realpath(nonexistent) returned non-NULL")
	}
	// NULL path -> NULL (EINVAL); empty path -> NULL (ENOENT). Without the guards
	// filepath.Abs("") resolves to the cwd and wrongly succeeds.
	if Realpath(nil, &buf[0]) != nil {
		t.Error("Realpath(NULL) returned non-NULL (should be EINVAL)")
	}
	if Realpath(csb(""), &buf[0]) != nil {
		t.Error("Realpath(\"\") returned non-NULL (should be ENOENT)")
	}
}
