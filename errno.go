// errno.go — per-goroutine errno via GLS.
//
// The C-side `errno` macro expands to `*__errno_location()`, and c2go routes the
// standard glibc/musl accessor name __errno_location to ErrnoPtr below (see the
// c2go_linkname on __errno_location in <c2go.h>). It returns the address of the
// calling goroutine's int32 errno so ported C source that touches errno resolves
// to the same per-goroutine slot the `errno` macro reads.
package libc

import (
	"syscall"

	_ "unsafe"
)

//go:linkname ErrnoPtr
func ErrnoPtr() *int32 {
	ts := glsRegisterIfAbsent()
	return &ts.errno
}

// setErrno writes errno for the current goroutine (used by the syscall shim
// wrappers to report failure; see the C-calls-Go bridge).
func setErrno(e int32) {
	glsRegisterIfAbsent().errno = e
}

// errnoOf extracts the native errno from a syscall error (EIO as a fallback
// for the rare non-Errno error). Cross-platform (#678: popen.go needs it on
// windows too; moved here from the unix-only io.go).
func errnoOf(err error) int32 {
	if e, ok := err.(syscall.Errno); ok {
		return int32(e)
	}
	return int32(syscall.EIO)
}
