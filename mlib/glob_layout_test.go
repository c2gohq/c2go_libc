// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"testing"
	"unsafe"
)

func TestManagedGlobLayouts(t *testing.T) {
	var result mlib_glob_t
	if got := unsafe.Sizeof(result); got != 72 {
		t.Fatalf("sizeof(mlib_glob_t) = %d, want 72", got)
	}
	if got := unsafe.Offsetof(result.gl_pathc); got != 0 {
		t.Fatalf("offsetof(gl_pathc) = %d, want 0", got)
	}
	if got := unsafe.Offsetof(result.gl_pathv); got != 8 {
		t.Fatalf("offsetof(gl_pathv) = %d, want 8", got)
	}
	if got := unsafe.Offsetof(result.gl_offs); got != 16 {
		t.Fatalf("offsetof(gl_offs) = %d, want 16", got)
	}
	if got := unsafe.Offsetof(result.__dummy1); got != 24 {
		t.Fatalf("offsetof(__dummy1) = %d, want 24", got)
	}
	if got := unsafe.Offsetof(result.__dummy2); got != 32 {
		t.Fatalf("offsetof(__dummy2) = %d, want 32", got)
	}
	if got := unsafe.Sizeof(mlib_glob_slot{}); got != unsafe.Sizeof(uintptr(0)) {
		t.Fatalf("sizeof(mlib_glob_slot) = %d, want one pointer", got)
	}
}
