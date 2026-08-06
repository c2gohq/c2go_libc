package libc

import (
	"runtime"
	"testing"
	"unsafe"
)

// These records cross generated Go declarations by value.  A missing sidecar
// type used to make c2go-bind fall back to struct{}, silently collapsing their
// ABI0 argument/result slots to zero bytes.
func TestGeneratedByValueRecordLayouts(t *testing.T) {
	word := unsafe.Sizeof(uintptr(0))
	checks := []struct {
		name string
		got  uintptr
		want uintptr
	}{
		{"cookie_io_functions_t", unsafe.Sizeof(cookie_io_functions_t{}), 4 * word},
		{"entry", unsafe.Sizeof(entry{}), 2 * word},
		{"div_t", unsafe.Sizeof(div_t{}), 8},
		{"imaxdiv_t", unsafe.Sizeof(imaxdiv_t{}), 16},
		{"lldiv_t", unsafe.Sizeof(lldiv_t{}), 16},
	}
	ldivSize := uintptr(16)
	if runtime.GOOS == "windows" { // LLP64: C long remains 32 bits.
		ldivSize = 8
	}
	checks = append(checks, struct {
		name string
		got  uintptr
		want uintptr
	}{"ldiv_t", unsafe.Sizeof(ldiv_t{}), ldivSize})

	for _, check := range checks {
		if check.got != check.want {
			t.Errorf("sizeof(%s) = %d, want %d", check.name, check.got, check.want)
		}
	}
}
