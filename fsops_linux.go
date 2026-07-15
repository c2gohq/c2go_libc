// fsops_linux.go — the per-OS fs bridges on Linux: fdatasync (the real
// syscall.Fdatasync) and isatty (probe with the linux TCGETS termios ioctl).
//
//go:build linux

package libc

import (
	"syscall"
	"unsafe"
)

//go:linkname __c2go_syscall_fdatasync
func __c2go_syscall_fdatasync(fd int32) int32 {
	if isStdFd(fd) {
		return stdSync(fd)
	}
	for {
		err := syscall.Fdatasync(int(fd))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_isatty
func __c2go_syscall_isatty(fd int32) int32 {
	if isStdFd(fd) {
		return stdIsatty(fd) // identity-gated: swapped std stream is never a tty
	}
	return rawIsatty(fd)
}

// rawIsatty is the un-routed probe (also the Control-side worker for a
// virtualized std descriptor — see stdIsatty).
func rawIsatty(fd int32) int32 {
	var t [128]byte // >= sizeof(struct termios); the kernel writes only its size
	_, _, e := syscall.Syscall(syscall.SYS_IOCTL, uintptr(fd),
		0x5401 /*TCGETS*/, uintptr(unsafe.Pointer(&t[0])))
	if e == 0 {
		return 1
	}
	if e == syscall.EBADF {
		return -int32(syscall.EBADF) // musl isatty preserves EBADF (#657)
	}
	return 0
}
