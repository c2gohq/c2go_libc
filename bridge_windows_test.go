//go:build windows

package libc

import (
	"os"
	"path/filepath"
	"testing"
	"unsafe"
)

// TestWindowsDirentBridge exercises the public C wrapper rather than calling
// the Go bridge directly.  It catches LLP64 mismatches between a C handle slot
// and the int64 handle consumed by the Go ABI0 wrapper.
func TestWindowsDirentBridge(t *testing.T) {
	d := Opendir(csb(os.TempDir()))
	if d == nil {
		t.Fatal("Opendir returned nil")
	}
	closed := false
	defer func() {
		if !closed {
			Closedir(d)
		}
	}()

	readName := func() string {
		de := Readdir(d)
		if de == nil {
			t.Fatal("Readdir returned nil")
		}
		cd := (*cDirent)(unsafe.Pointer(de))
		return cstr(&cd.name[0])
	}
	for _, want := range []string{".", ".."} {
		if got := readName(); got != want {
			t.Fatalf("Readdir returned %q, want %q", got, want)
		}
	}
	Rewinddir(d)
	if got := readName(); got != "." {
		t.Fatalf("Readdir after Rewinddir returned %q, want dot", got)
	}
	if rc := Closedir(d); rc != 0 {
		t.Fatalf("Closedir returned %d", rc)
	}
	closed = true
}

// TestWindowsDirentEnumeration covers the OS-backed part of Readdir on a
// native Windows runner.  The local Whisky Wine build lacks
// GetFileInformationByHandleEx, so it can exercise the bridge above but not
// os.File.ReadDir itself.
func TestWindowsDirentEnumeration(t *testing.T) {
	if os.Getenv("WINEPREFIX") != "" {
		t.Skip("Wine does not implement the directory query used by os.File.ReadDir")
	}
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "entry"), []byte("x"), 0o600); err != nil {
		t.Fatal(err)
	}
	d := Opendir(csb(dir))
	if d == nil {
		t.Fatal("Opendir returned nil")
	}
	defer Closedir(d)

	for {
		de := Readdir(d)
		if de == nil {
			break
		}
		cd := (*cDirent)(unsafe.Pointer(de))
		if cstr(&cd.name[0]) == "entry" {
			return
		}
	}
	t.Fatal("Readdir did not return the created entry")
}

// TestWindowsConfBridges keeps the cross-platform conf wrappers in the native
// Windows test set; their shared Unix test file is excluded by its build tag.
func TestWindowsConfBridges(t *testing.T) {
	if ps := Getpagesize(); ps < 4096 {
		t.Fatalf("Getpagesize = %d", ps)
	}
	if got := Sysconf(30); got != int32(Getpagesize()) {
		t.Fatalf("Sysconf(_SC_PAGESIZE) = %d", got)
	}
	if got := Sysconf(84); got < 1 {
		t.Fatalf("Sysconf(_SC_NPROCESSORS_ONLN) = %d", got)
	}
	if got := Sysconf(9999); got != -1 {
		t.Fatalf("Sysconf(unknown) = %d", got)
	}

	var host [256]byte
	if rc := Gethostname(&host[0], uint64(len(host))); rc != 0 || cstr(&host[0]) == "" {
		t.Fatalf("Gethostname rc=%d host=%q", rc, cstr(&host[0]))
	}
	var entropy [16]byte
	if rc := Getentropy(unsafe.Pointer(&entropy[0]), uint64(len(entropy))); rc != 0 {
		t.Fatalf("Getentropy returned %d", rc)
	}
	if entropy == [16]byte{} {
		t.Fatal("Getentropy returned all zeros")
	}
}
