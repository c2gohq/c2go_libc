// fsops_test.go — runtime tests for the extended POSIX fd/path ops
// (source/fsops_posix.c + fsops.go). Each op is checked against an independent Go
// path (os package) or a round-trip through a second op.
//
//go:build unix

package libc

import (
	"os"
	"path/filepath"
	"runtime"
	"testing"
	"unsafe"
)

// fsRDWR is 2 on both darwin and linux.
const fsRDWR = 2

// openRDWR opens path through our own open() and returns the fd.
func openRDWR(t *testing.T, path string) int32 {
	t.Helper()
	pb, pp := cbytes(path)
	fd := Open(pp, fsRDWR, nil)
	runtime.KeepAlive(pb)
	if fd < 0 {
		t.Fatalf("Open(%s) = %d", path, fd)
	}
	return fd
}

func TestPreadPwrite(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("hello world"), 0o644); err != nil {
		t.Fatal(err)
	}
	fd := openRDWR(t, path)
	defer Close(fd)

	// pread 5 bytes at offset 6 -> "world" (and it must not move the fd offset).
	buf := make([]byte, 5)
	if n := Pread(fd, unsafe.Pointer(&buf[0]), 5, 6); n != 5 {
		t.Fatalf("Pread = %d, want 5", n)
	}
	if string(buf) != "world" {
		t.Errorf("Pread got %q, want %q", buf, "world")
	}
	// pwrite "WORLD" at offset 6.
	w := []byte("WORLD")
	if n := Pwrite(fd, unsafe.Pointer(&w[0]), 5, 6); n != 5 {
		t.Fatalf("Pwrite = %d, want 5", n)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(data) != "hello WORLD" {
		t.Errorf("after Pwrite file = %q, want %q", data, "hello WORLD")
	}
}

func TestDup(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("abc"), 0o644); err != nil {
		t.Fatal(err)
	}
	fd := openRDWR(t, path)
	defer Close(fd)
	nfd := Dup(fd)
	if nfd < 0 {
		t.Fatalf("Dup = %d", nfd)
	}
	defer Close(nfd)
	buf := make([]byte, 3)
	if n := Read(nfd, unsafe.Pointer(&buf[0]), 3); n != 3 || string(buf) != "abc" {
		t.Errorf("Read via dup = %d %q, want 3 %q", n, buf, "abc")
	}
}

func TestPipe(t *testing.T) {
	var fds [2]int32
	if r := Pipe(&fds[0]); r != 0 {
		t.Fatalf("Pipe = %d", r)
	}
	defer Close(fds[0])
	defer Close(fds[1])
	msg := []byte("ping")
	if n := Write(fds[1], unsafe.Pointer(&msg[0]), 4); n != 4 {
		t.Fatalf("Write to pipe = %d", n)
	}
	buf := make([]byte, 4)
	if n := Read(fds[0], unsafe.Pointer(&buf[0]), 4); n != 4 || string(buf) != "ping" {
		t.Errorf("Read from pipe = %d %q, want 4 %q", n, buf, "ping")
	}
}

func TestFsyncFdatasync(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("data"), 0o644); err != nil {
		t.Fatal(err)
	}
	fd := openRDWR(t, path)
	defer Close(fd)
	msg := []byte("more")
	Write(fd, unsafe.Pointer(&msg[0]), 4)
	if r := Fsync(fd); r != 0 {
		t.Errorf("Fsync = %d, want 0", r)
	}
	if r := Fdatasync(fd); r != 0 {
		t.Errorf("Fdatasync = %d, want 0", r)
	}
}

func TestTruncateFtruncate(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("0123456789"), 0o644); err != nil {
		t.Fatal(err)
	}
	fd := openRDWR(t, path)
	defer Close(fd)

	if r := Ftruncate(fd, 4); r != 0 {
		t.Fatalf("Ftruncate = %d", r)
	}
	if m, _ := doStat(path); m.size != 4 {
		t.Errorf("after Ftruncate size = %d, want 4", m.size)
	}

	tb, tp := cbytes(path)
	if r := Truncate(tp, 8); r != 0 {
		t.Fatalf("Truncate = %d", r)
	}
	runtime.KeepAlive(tb)
	if m, _ := doStat(path); m.size != 8 {
		t.Errorf("after Truncate size = %d, want 8", m.size)
	}
	// Extension is zero-filled; original 4 bytes preserved.
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if want := "0123\x00\x00\x00\x00"; string(data) != want {
		t.Errorf("after Truncate file = %q, want %q", data, want)
	}
}

func TestLinkSymlinkReadlink(t *testing.T) {
	dir := t.TempDir()
	a := filepath.Join(dir, "a")
	b := filepath.Join(dir, "b")
	if err := os.WriteFile(a, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	ab, ap := cbytes(a)
	bb, bp := cbytes(b)
	if r := Link(ap, bp); r != 0 {
		t.Fatalf("Link = %d", r)
	}
	runtime.KeepAlive(ab)
	runtime.KeepAlive(bb)
	ma, _ := doStat(a)
	mb, _ := doStat(b)
	if ma.ino != mb.ino || ma.ino == 0 {
		t.Errorf("Link: ino a=%d b=%d, want equal nonzero", ma.ino, mb.ino)
	}
	if ma.nlink != 2 {
		t.Errorf("Link: nlink = %d, want 2", ma.nlink)
	}

	tgt := filepath.Join(dir, "tgt")
	lnk := filepath.Join(dir, "lnk")
	if err := os.WriteFile(tgt, []byte("y"), 0o644); err != nil {
		t.Fatal(err)
	}
	tb, tp := cbytes(tgt)
	lb, lp := cbytes(lnk)
	if r := Symlink(tp, lp); r != 0 {
		t.Fatalf("Symlink = %d", r)
	}
	runtime.KeepAlive(tb)
	runtime.KeepAlive(lb)

	var rbuf [512]byte
	n := Readlink(lp, &rbuf[0], 512)
	if n <= 0 {
		t.Fatalf("Readlink = %d", n)
	}
	if got := string(rbuf[:n]); got != tgt {
		t.Errorf("Readlink = %q, want %q", got, tgt)
	}
}

func TestChdirGetcwd(t *testing.T) {
	orig, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	defer os.Chdir(orig) // restore process cwd for other tests

	dir := t.TempDir()
	db, dp := cbytes(dir)
	if r := Chdir(dp); r != 0 {
		t.Fatalf("Chdir = %d", r)
	}
	runtime.KeepAlive(db)

	var buf [4096]byte
	if p := Getcwd(&buf[0], 4096); p == nil {
		t.Fatal("Getcwd returned NULL")
	}
	got := cstr(&buf[0])
	// os.Getwd now resolves to the same directory via the same getcwd syscall.
	want, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if got != want {
		t.Errorf("Getcwd = %q, want %q", got, want)
	}

	// Buffer too small -> NULL (ERANGE).
	var tiny [1]byte
	if p := Getcwd(&tiny[0], 1); p != nil {
		t.Error("Getcwd(size=1) should fail with ERANGE, got non-NULL")
	}
}

func TestIsatty(t *testing.T) {
	// A regular file is not a terminal.
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	fd := openRDWR(t, path)
	defer Close(fd)
	if r := Isatty(fd); r != 0 {
		t.Errorf("Isatty(regular file) = %d, want 0", r)
	}
	// A pipe end is not a terminal either.
	var fds [2]int32
	if Pipe(&fds[0]) == 0 {
		defer Close(fds[0])
		defer Close(fds[1])
		if r := Isatty(fds[0]); r != 0 {
			t.Errorf("Isatty(pipe) = %d, want 0", r)
		}
	}
}
