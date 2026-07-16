// ioctl_unix.go — #675 stage D: the ioctl syscall shim. The C wrapper
// (source/ioctl.c) whitelists the commands and extracts the vararg by kind
// (pointer vs int) before calling here, so req/arg pass through to the
// kernel raw — the C-side structs are kernel-layout by construction (the
// per-OS <termios.h>/<sys/ioctl.h>). Shim contract mirrors io.go: return
// the result or -errno. Goes through x/sys/unix like the *at family —
// darwin's syscall package has no SYS_ numbers (unix.Syscall routes through
// libSystem there).

//go:build unix

package libc

import (
	"unsafe"

	"golang.org/x/sys/unix"
)

//go:linkname __c2go_syscall_ioctl
func __c2go_syscall_ioctl(fd int32, req int32, arg unsafe.Pointer) int64 {
	// uint32 first: darwin commands set bit 31 (IOC_IN), and the kernel takes
	// an unsigned long — sign-extending req would garbage the high word.
	for {
		r, _, e := unix.Syscall(unix.SYS_IOCTL, uintptr(fd), uintptr(uint32(req)), uintptr(arg))
		if e == unix.EINTR {
			continue // spurious runtime-preemption EINTR (see io.go header)
		}
		if e != 0 {
			return -int64(e)
		}
		return int64(r)
	}
}
