// futimens_linux.go — futimens = utimensat(fd, NULL, times, 0) (the musl
// spelling). The Go wrappers cannot express a NULL path (string-typed), so
// this goes through the raw utimensat syscall with path=0 — the linux
// kernel's documented "operate on dirfd itself" form.

//go:build linux

package libc

import (
	"syscall"
	"unsafe"
)

//go:linkname __c2go_syscall_futimens
func __c2go_syscall_futimens(fd int32, times unsafe.Pointer) int64 {
	for {
		_, _, e := syscall.Syscall6(syscall.SYS_UTIMENSAT,
			uintptr(fd), 0 /* NULL path */, uintptr(times), 0, 0, 0)
		if e == syscall.EINTR {
			continue
		}
		if e != 0 {
			return -int64(e)
		}
		return 0
	}
}
