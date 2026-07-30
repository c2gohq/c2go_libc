package libc

// Golden tests for the #609 filesystem operations: remove / rename (stdio path
// ops layered on the per-OS unlink/rmdir/rename primitives) and tmpfile (an
// anonymous read/write stream via io.go's tmpfileFd). Ground truth is the host
// filesystem through the os package — after each libc call, os.Stat and file
// contents must reflect it.

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"unsafe"
)

const seekSet = 0 // <stdio.h> SEEK_SET

// TestRemoveFile: Remove deletes a regular file; a second Remove then fails.
func TestRemoveFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "victim.txt")
	if err := os.WriteFile(path, []byte("bye"), 0644); err != nil {
		t.Fatal(err)
	}
	pb, pp := cbytes(path)
	if r := Remove(pp); r != 0 {
		t.Fatalf("Remove = %d, want 0", r)
	}
	runtime.KeepAlive(pb)
	if _, err := os.Stat(path); !os.IsNotExist(err) {
		t.Errorf("file still present after Remove: %v", err)
	}
	pb2, pp2 := cbytes(path)
	if r := Remove(pp2); r == 0 {
		t.Error("Remove of a missing path returned 0, want -1")
	}
	runtime.KeepAlive(pb2)
}

// TestRemoveDir: on unix Remove(dir) unlink-fails with EISDIR and falls back to
// rmdir (musl's remove). (Windows _unlink reports EACCES, so remove is file-only
// there; this test is meaningful on the darwin gate.)
func TestRemoveDir(t *testing.T) {
	dir := filepath.Join(t.TempDir(), "sub")
	if err := os.Mkdir(dir, 0755); err != nil {
		t.Fatal(err)
	}
	pb, pp := cbytes(dir)
	if r := Remove(pp); r != 0 {
		t.Fatalf("Remove(dir) = %d, want 0", r)
	}
	runtime.KeepAlive(pb)
	if _, err := os.Stat(dir); !os.IsNotExist(err) {
		t.Errorf("dir still present after Remove: %v", err)
	}
}

// TestRename: Rename moves content old→new; the old name vanishes.
func TestRename(t *testing.T) {
	dir := t.TempDir()
	oldp := filepath.Join(dir, "old.txt")
	newp := filepath.Join(dir, "new.txt")
	const content = "rename payload\n"
	if err := os.WriteFile(oldp, []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
	ob, op := cbytes(oldp)
	nb, np := cbytes(newp)
	if r := Rename(op, np); r != 0 {
		t.Fatalf("Rename = %d, want 0", r)
	}
	runtime.KeepAlive(ob)
	runtime.KeepAlive(nb)
	if _, err := os.Stat(oldp); !os.IsNotExist(err) {
		t.Errorf("old path still present after Rename: %v", err)
	}
	got, err := os.ReadFile(newp)
	if err != nil {
		t.Fatalf("reading renamed file: %v", err)
	}
	if string(got) != content {
		t.Errorf("renamed content = %q, want %q", got, content)
	}
}

// TestUnlinkRmdir exercises the <unistd.h> primitives directly.
func TestUnlinkRmdir(t *testing.T) {
	dir := t.TempDir()
	file := filepath.Join(dir, "f")
	sub := filepath.Join(dir, "d")
	if err := os.WriteFile(file, []byte("x"), 0644); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(sub, 0755); err != nil {
		t.Fatal(err)
	}
	fb, fp := cbytes(file)
	if r := Unlink(fp); r != 0 {
		t.Fatalf("Unlink = %d, want 0", r)
	}
	runtime.KeepAlive(fb)
	if _, err := os.Stat(file); !os.IsNotExist(err) {
		t.Error("file survives Unlink")
	}
	sb, sp := cbytes(sub)
	if r := Rmdir(sp); r != 0 {
		t.Fatalf("Rmdir = %d, want 0", r)
	}
	runtime.KeepAlive(sb)
	if _, err := os.Stat(sub); !os.IsNotExist(err) {
		t.Error("dir survives Rmdir")
	}
}

// TestTmpfile: an anonymous read/write stream. A write then rewind-read must
// round-trip, and Ftell must track the write; the backing file is unlinked at
// creation, so no "tmpfile*" name appears in the temp dir while it is open.
func TestTmpfile(t *testing.T) {
	before := tmpfileEntries(t)

	f := Tmpfile()
	if f == nil {
		t.Fatal("Tmpfile returned nil")
	}
	defer Fclose(f)

	const payload = "temp stream contents 0123456789\n"
	data := []byte(payload)
	n := Fwrite(unsafe.Pointer(&data[0]), 1, uint64(len(data)), f)
	runtime.KeepAlive(data)
	if n != uint64(len(data)) {
		t.Fatalf("Fwrite wrote %d, want %d", n, len(data))
	}
	if pos := Ftell(f); int64(pos) != int64(len(data)) {
		t.Errorf("Ftell after write = %d, want %d", pos, len(data))
	}
	if r := Fseek(f, 0, seekSet); r != 0 {
		t.Fatalf("Fseek(0) = %d", r)
	}
	buf := make([]byte, len(data))
	m := Fread(unsafe.Pointer(&buf[0]), 1, uint64(len(buf)), f)
	runtime.KeepAlive(buf)
	if m != uint64(len(buf)) {
		t.Fatalf("Fread read %d, want %d", m, len(buf))
	}
	if string(buf) != payload {
		t.Errorf("tmpfile roundtrip = %q, want %q", buf, payload)
	}

	// The stream is anonymous: its on-disk name was unlinked at creation, so no
	// new tmpfile-prefixed entry should have appeared in the temp dir.
	for name := range tmpfileEntries(t) {
		if _, seen := before[name]; !seen {
			t.Errorf("Tmpfile leaked a visible temp entry %q (should be unlinked)", name)
		}
	}
}

// tmpfileEntries returns the set of /tmp entries named like the C tmpfile's
// template ("tmpfile_XXXXXX", musl's src/stdio/tmpfile.c); used to prove Tmpfile
// leaves nothing on disk. tmpfile hardcodes /tmp (musl-faithful), so scan there
// — not os.TempDir(), which on macOS is a per-user dir.
func tmpfileEntries(t *testing.T) map[string]struct{} {
	t.Helper()
	set := map[string]struct{}{}
	ents, err := os.ReadDir("/tmp")
	if err != nil {
		return set
	}
	for _, e := range ents {
		if strings.HasPrefix(e.Name(), "tmpfile_") {
			set[e.Name()] = struct{}{}
		}
	}
	return set
}
