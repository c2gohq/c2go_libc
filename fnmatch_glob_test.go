//go:build !windows

package libc

// fnmatch + glob ABI smoke tests, extracted verbatim from OLD regex_test.go
// (#667) — the TRE regcomp/regexec tests stay behind until the regex cluster
// migrates. Semantic surface is covered by the dual-arch dual-O probe harness
// against the host-libc oracle (probe_fnmatch/probe_glob, see the #667 log).
// glob_t is a 0-byte opaque binding: allocate real backing and cast
// (quickbatch precedent — C writes through the pointer, Go owns the storage).

import (
	"os"
	"path/filepath"
	"testing"
	"unsafe"
)

func TestFnmatch(t *testing.T) {
	if rc := Fnmatch(csb("*.c"), csb("foo.c"), 0); rc != 0 {
		t.Fatalf("fnmatch(*.c, foo.c) = %d, want 0", rc)
	}
	if rc := Fnmatch(csb("*.c"), csb("foo.h"), 0); rc != 1 { // FNM_NOMATCH
		t.Fatalf("fnmatch(*.c, foo.h) = %d, want FNM_NOMATCH(1)", rc)
	}
	// FNM_PATHNAME(0x1): * must not cross '/'
	if rc := Fnmatch(csb("*"), csb("a/b"), 0x1); rc != 1 {
		t.Fatalf("fnmatch(*, a/b, FNM_PATHNAME) = %d, want 1", rc)
	}
}

// globT is 72-byte backing for the opaque glob_t (2*size_t + char** + int +
// reserved), 8-aligned.
type globT [9]uint64

func TestGlob(t *testing.T) {
	dir := t.TempDir()
	for _, f := range []string{"a.c", "b.c", "c.h"} {
		if err := os.WriteFile(filepath.Join(dir, f), nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	var gb globT
	g := (*glob_t)(unsafe.Pointer(&gb))
	if rc := Glob(csb(filepath.Join(dir, "*.c")), 0, 0, g); rc != 0 {
		t.Fatalf("glob = %d, want 0", rc)
	}
	pathc := gb[0]
	pathv := (*[4]*byte)(unsafe.Pointer(uintptr(gb[1])))
	if pathc != 2 {
		t.Fatalf("gl_pathc = %d, want 2", pathc)
	}
	if got0, got1 := cstr(pathv[0]), cstr(pathv[1]); filepath.Base(got0) != "a.c" || filepath.Base(got1) != "b.c" {
		t.Fatalf("gl_pathv = [%s %s], want sorted a.c b.c", got0, got1)
	}
	Globfree(g)
	if gb[0] != 0 || gb[1] != 0 {
		t.Fatalf("globfree left pathc=%d pathv=%#x", gb[0], gb[1])
	}
	// no match without NOCHECK -> GLOB_NOMATCH(3)
	if rc := Glob(csb(filepath.Join(dir, "*.zz")), 0, 0, g); rc != 3 {
		t.Fatalf("glob(*.zz) = %d, want GLOB_NOMATCH(3)", rc)
	}
}
