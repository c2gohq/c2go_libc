// cstr.go — shared C-string helper (cross-platform).
package libc

import "unsafe"

// cstr copies a NUL-terminated C string into a Go string (used for path/name
// args by the fd layer, the env bridge, etc.).
func cstr(p *byte) string {
	if p == nil {
		return ""
	}
	n := 0
	for *(*byte)(unsafe.Add(unsafe.Pointer(p), n)) != 0 {
		n++
	}
	return string(unsafe.Slice(p, n))
}
