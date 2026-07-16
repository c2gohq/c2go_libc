//go:build unix

package libc

// Tranche C2: directory streams (source/dirent.c + dirent.go). Reaches the C
// impls through the c2go-bind bindings; the opaque *dirent result is read back
// through the matching cDirent layout (same trick stat's tests would use).

import (
	"os"
	"path/filepath"
	"testing"
	"unsafe"
)

func TestDirent(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "a.txt"), []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(filepath.Join(dir, "sub"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink("a.txt", filepath.Join(dir, "lnk")); err != nil {
		t.Fatal(err)
	}

	d := Opendir(csb(dir))
	if d == nil {
		t.Fatal("Opendir returned NULL")
	}
	defer Closedir(d)

	got := map[string]uint8{}
	for {
		de := Readdir(d)
		if de == nil {
			break
		}
		cd := (*cDirent)(unsafe.Pointer(de))
		got[cstr(&cd.name[0])] = cd.typ
	}

	// POSIX readdir yields "." and ".." (Go's os.ReadDir omits them; the shim
	// re-adds them).
	for _, dot := range []string{".", ".."} {
		if _, ok := got[dot]; !ok {
			t.Errorf("readdir missing %q", dot)
		}
	}
	if got["a.txt"] != 8 { // DT_REG
		t.Errorf("a.txt d_type = %d, want DT_REG(8)", got["a.txt"])
	}
	if got["sub"] != 4 { // DT_DIR
		t.Errorf("sub d_type = %d, want DT_DIR(4)", got["sub"])
	}
	if got["lnk"] != 10 { // DT_LNK
		t.Errorf("lnk d_type = %d, want DT_LNK(10)", got["lnk"])
	}
	if len(got) != 5 {
		t.Errorf("entry count = %d, want 5 (. .. a.txt sub lnk); got %v", len(got), got)
	}

	// rewinddir: after exhausting the stream, rewind and the first entry is "."
	// again.
	Rewinddir(d)
	de := Readdir(d)
	if de == nil {
		t.Fatal("Readdir after Rewinddir returned NULL")
	}
	if name := cstr(&(*cDirent)(unsafe.Pointer(de)).name[0]); name != "." {
		t.Errorf("first entry after rewind = %q, want \".\"", name)
	}
}

func TestReaddirR(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "only"), []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	d := Opendir(csb(dir))
	if d == nil {
		t.Fatal("Opendir returned NULL")
	}
	defer Closedir(d)

	seen := map[string]bool{}
	for {
		var buf cDirent // a real-size buffer; the opaque `dirent` type is 0 bytes
		var result *dirent
		if r := ReaddirR(d, (*dirent)(unsafe.Pointer(&buf)), &result); r != 0 {
			t.Fatalf("ReaddirR returned errno %d", r)
		}
		if result == nil { // end of directory
			break
		}
		seen[cstr(&buf.name[0])] = true
	}
	for _, want := range []string{".", "..", "only"} {
		if !seen[want] {
			t.Errorf("ReaddirR missing %q", want)
		}
	}
}

func TestOpendirErrors(t *testing.T) {
	// Nonexistent directory -> NULL.
	if Opendir(csb(filepath.Join(t.TempDir(), "nope"))) != nil {
		t.Error("Opendir(nonexistent) returned non-NULL")
	}
	// A regular file is not a directory -> NULL (ENOTDIR).
	f := filepath.Join(t.TempDir(), "file")
	if err := os.WriteFile(f, []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	if Opendir(csb(f)) != nil {
		t.Error("Opendir(regular file) returned non-NULL")
	}
}
