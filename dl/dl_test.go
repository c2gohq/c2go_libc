package dl_test

import (
	"runtime"
	"testing"
	"unsafe"

	"github.com/c2gohq/c2go_libc/dl"
)

func defaultLibcPath() string {
	switch runtime.GOOS {
	case "darwin":
		return "/usr/lib/libSystem.B.dylib"
	case "linux":
		return "libc.so.6"
	case "windows":
		return "msvcrt.dll"
	}
	return ""
}

// TestDlsymPath exercises path 2 of doc.go — runtime Dlopen + Dlsym.
func TestDlsymPath(t *testing.T) {
	p := defaultLibcPath()
	if p == "" {
		t.Skipf("no default libc path for %s", runtime.GOOS)
	}
	h, err := dl.Dlopen(p, dl.RTLD_LAZY|dl.RTLD_GLOBAL)
	if err != nil {
		t.Fatalf("Dlopen(%q): %v", p, err)
	}
	defer func() {
		if err := dl.Dlclose(h); err != nil {
			t.Errorf("Dlclose: %v", err)
		}
	}()
	strlen, err := dl.Dlsym(h, "strlen")
	if err != nil {
		t.Fatalf("Dlsym(strlen): %v", err)
	}
	s := []byte("Hello, c2go ext bridge!\x00")
	r1, _, _ := dl.SyscallN(strlen, uintptr(unsafe.Pointer(&s[0])))
	runtime.KeepAlive(s)
	want := uintptr(len(s) - 1)
	if r1 != want {
		t.Fatalf("Dlsym-path strlen got %d want %d", r1, want)
	}
}
