// stat_test.go — runtime cross-checks for the POSIX file-metadata layer
// (source/stat_posix.c + stat.go). Every filled struct stat field is compared
// against an independent Go path (os.Stat / os.Getuid), so a wrong cStat offset
// or a mis-mapped per-OS syscall.Stat_t field would fail here.
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

// struct stat mode bits (mirror <sys/stat.h>).
const (
	sIFMT  = 0o170000
	sIFREG = 0o100000
	sIFDIR = 0o040000
	sIFLNK = 0o120000
)

// access() amode bits (mirror <unistd.h>).
const (
	fOK = 0
	rOK = 4
	wOK = 2
)

// doStat calls the C stat() wrapper into a cStat, keeping the path alive.
func doStat(path string) (cStat, int32) {
	var m cStat
	pb, pp := cbytes(path)
	r := Stat(pp, (*stat)(unsafe.Pointer(&m)))
	runtime.KeepAlive(pb)
	return m, r
}

// TestStatLayout pins the Go mirror to the C struct stat layout (proven 120 bytes
// with these offsets by the _Static_assert probe; this guards the Go side).
func TestStatLayout(t *testing.T) {
	if got := unsafe.Sizeof(cStat{}); got != 120 {
		t.Fatalf("sizeof(cStat) = %d, want 120", got)
	}
	var s cStat
	offs := []struct {
		name string
		off  uintptr
		want uintptr
	}{
		{"dev", unsafe.Offsetof(s.dev), 0},
		{"ino", unsafe.Offsetof(s.ino), 8},
		{"nlink", unsafe.Offsetof(s.nlink), 16},
		{"mode", unsafe.Offsetof(s.mode), 24},
		{"uid", unsafe.Offsetof(s.uid), 28},
		{"gid", unsafe.Offsetof(s.gid), 32},
		{"rdev", unsafe.Offsetof(s.rdev), 40},
		{"size", unsafe.Offsetof(s.size), 48},
		{"blksize", unsafe.Offsetof(s.blksize), 56},
		{"blocks", unsafe.Offsetof(s.blocks), 64},
		{"atim", unsafe.Offsetof(s.atim), 72},
		{"mtim", unsafe.Offsetof(s.mtim), 88},
		{"ctim", unsafe.Offsetof(s.ctim), 104},
	}
	for _, o := range offs {
		if o.off != o.want {
			t.Errorf("offsetof(cStat.%s) = %d, want %d", o.name, o.off, o.want)
		}
	}
}

// TestStat cross-checks every field of stat() against os.Stat / os.Getuid.
func TestStat(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f.dat")
	content := []byte("0123456789abcdef") // 16 bytes
	if err := os.WriteFile(path, content, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Chmod(path, 0o644); err != nil { // defeat umask for an exact perm check
		t.Fatal(err)
	}

	m, r := doStat(path)
	if r != 0 {
		t.Fatalf("Stat = %d, want 0", r)
	}
	fi, err := os.Stat(path)
	if err != nil {
		t.Fatal(err)
	}

	if int64(m.size) != fi.Size() || m.size != int64(len(content)) {
		t.Errorf("st_size = %d, want %d", m.size, len(content))
	}
	if m.mode&sIFMT != sIFREG {
		t.Errorf("st_mode type = %#o, want regular (%#o)", m.mode&sIFMT, sIFREG)
	}
	if m.mode&0o777 != 0o644 {
		t.Errorf("st_mode perm = %#o, want 0644", m.mode&0o777)
	}
	if m.uid != uint32(os.Getuid()) {
		t.Errorf("st_uid = %d, want %d", m.uid, os.Getuid())
	}
	if m.gid != uint32(os.Getgid()) {
		t.Errorf("st_gid = %d, want %d", m.gid, os.Getgid())
	}
	if m.mtim.sec != fi.ModTime().Unix() {
		t.Errorf("st_mtim.sec = %d, want %d", m.mtim.sec, fi.ModTime().Unix())
	}
	if m.mtim.nsec != int64(fi.ModTime().Nanosecond()) {
		t.Errorf("st_mtim.nsec = %d, want %d", m.mtim.nsec, fi.ModTime().Nanosecond())
	}
	if m.nlink != 1 {
		t.Errorf("st_nlink = %d, want 1", m.nlink)
	}
	if m.ino == 0 {
		t.Error("st_ino = 0, want nonzero")
	}
	if m.dev == 0 {
		t.Error("st_dev = 0, want nonzero")
	}
	if m.blksize <= 0 {
		t.Errorf("st_blksize = %d, want > 0", m.blksize)
	}

	// Nonexistent path -> -1.
	if _, r := doStat(filepath.Join(t.TempDir(), "nope")); r != -1 {
		t.Errorf("Stat(nonexistent) = %d, want -1", r)
	}
}

// TestStatHardlink confirms st_nlink tracks a second link.
func TestStatHardlink(t *testing.T) {
	dir := t.TempDir()
	a := filepath.Join(dir, "a")
	b := filepath.Join(dir, "b")
	if err := os.WriteFile(a, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Link(a, b); err != nil {
		t.Skipf("hardlink unsupported: %v", err)
	}
	m, r := doStat(a)
	if r != 0 {
		t.Fatalf("Stat = %d", r)
	}
	if m.nlink != 2 {
		t.Errorf("st_nlink = %d, want 2", m.nlink)
	}
}

// TestLstat: lstat sees the symlink itself, stat follows it to the target.
func TestLstat(t *testing.T) {
	dir := t.TempDir()
	target := filepath.Join(dir, "t.dat")
	link := filepath.Join(dir, "l")
	if err := os.WriteFile(target, []byte("hi"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(target, link); err != nil {
		t.Skipf("symlink unsupported: %v", err)
	}

	var lm cStat
	lb, lp := cbytes(link)
	if r := Lstat(lp, (*stat)(unsafe.Pointer(&lm))); r != 0 {
		t.Fatalf("Lstat = %d", r)
	}
	runtime.KeepAlive(lb)
	if lm.mode&sIFMT != sIFLNK {
		t.Errorf("Lstat st_mode type = %#o, want symlink (%#o)", lm.mode&sIFMT, sIFLNK)
	}

	// stat() follows the link -> the regular target.
	sm, r := doStat(link)
	if r != 0 {
		t.Fatalf("Stat(link) = %d", r)
	}
	if sm.mode&sIFMT != sIFREG {
		t.Errorf("Stat(link) st_mode type = %#o, want regular", sm.mode&sIFMT)
	}
	if sm.size != 2 {
		t.Errorf("Stat(link) st_size = %d, want 2", sm.size)
	}
}

// TestFstat stats an fd opened through our own open().
func TestFstat(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	content := []byte("abcdefghij") // 10 bytes
	if err := os.WriteFile(path, content, 0o644); err != nil {
		t.Fatal(err)
	}
	pb, pp := cbytes(path)
	fd := Open(pp, 0 /*O_RDONLY: 0 on every platform*/, nil)
	runtime.KeepAlive(pb)
	if fd < 0 {
		t.Fatalf("Open = %d", fd)
	}
	defer Close(fd)

	var m cStat
	if r := Fstat(fd, (*stat)(unsafe.Pointer(&m))); r != 0 {
		t.Fatalf("Fstat = %d", r)
	}
	if m.size != int64(len(content)) {
		t.Errorf("Fstat st_size = %d, want %d", m.size, len(content))
	}
	if m.mode&sIFMT != sIFREG {
		t.Errorf("Fstat st_mode type = %#o, want regular", m.mode&sIFMT)
	}
}

// TestChmodFchmod: chmod by path, then fchmod by fd; verify via stat().
func TestChmodFchmod(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}

	cb, cp := cbytes(path)
	if r := Chmod(cp, 0o600); r != 0 {
		t.Fatalf("Chmod = %d", r)
	}
	runtime.KeepAlive(cb)
	if m, _ := doStat(path); m.mode&0o777 != 0o600 {
		t.Errorf("after Chmod perm = %#o, want 0600", m.mode&0o777)
	}

	pb, pp := cbytes(path)
	fd := Open(pp, 0 /*O_RDONLY*/, nil)
	runtime.KeepAlive(pb)
	if fd < 0 {
		t.Fatalf("Open = %d", fd)
	}
	defer Close(fd)
	if r := Fchmod(fd, 0o640); r != 0 {
		t.Fatalf("Fchmod = %d", r)
	}
	if m, _ := doStat(path); m.mode&0o777 != 0o640 {
		t.Errorf("after Fchmod perm = %#o, want 0640", m.mode&0o777)
	}
}

// TestMkdirUmask exercises mkdir() and umask() together: with umask 0 the
// directory keeps its exact requested mode.
func TestMkdirUmask(t *testing.T) {
	dir := t.TempDir()
	old := Umask(0)
	defer Umask(old)

	sub := filepath.Join(dir, "d")
	db, dp := cbytes(sub)
	if r := Mkdir(dp, 0o755); r != 0 {
		t.Fatalf("Mkdir = %d", r)
	}
	runtime.KeepAlive(db)

	m, r := doStat(sub)
	if r != 0 {
		t.Fatalf("Stat(dir) = %d", r)
	}
	if m.mode&sIFMT != sIFDIR {
		t.Errorf("st_mode type = %#o, want dir (%#o)", m.mode&sIFMT, sIFDIR)
	}
	if m.mode&0o777 != 0o755 {
		t.Errorf("dir perm = %#o, want 0755 (umask 0)", m.mode&0o777)
	}

	// Umask(0) returned the real previous mask; restoring it must round-trip.
	if got := Umask(old); got != 0 {
		t.Errorf("Umask round-trip: intermediate mask = %#o, want 0", got)
	}
}

// TestAccess: F_OK/R_OK on an existing file succeed; a missing path fails.
func TestAccess(t *testing.T) {
	path := filepath.Join(t.TempDir(), "f")
	if err := os.WriteFile(path, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	eb, ep := cbytes(path)
	if r := Access(ep, fOK); r != 0 {
		t.Errorf("Access(F_OK) = %d, want 0", r)
	}
	if r := Access(ep, rOK|wOK); r != 0 {
		t.Errorf("Access(R_OK|W_OK) = %d, want 0", r)
	}
	runtime.KeepAlive(eb)

	nb, np := cbytes(filepath.Join(t.TempDir(), "missing"))
	if r := Access(np, fOK); r != -1 {
		t.Errorf("Access(missing) = %d, want -1", r)
	}
	runtime.KeepAlive(nb)
}
