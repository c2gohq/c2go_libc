// io_linux.go — the dup2/dup3 syscall shims on Linux, which has the atomic dup3
// syscall (behind io_posix.c's dup2/dup3 wrappers). On arm64 there is no dup2
// syscall, so __c2go_syscall_dup2 is emulated via dup3 (musl src/unistd/dup2.c).
// Both return the new fd on success or -errno on failure (the shim contract in
// io.go); the C wrapper turns -errno into errno + -1.
//
//go:build linux

package libc

import (
	"syscall"
	_ "unsafe"
)

//go:linkname __c2go_syscall_dup2
func __c2go_syscall_dup2(oldfd, newfd int32) int32 {
	if r, done := stdDupGate(oldfd, newfd, 0); done {
		return r
	}
	return rawDup2(oldfd, newfd)
}

//go:linkname __c2go_syscall_dup3
func __c2go_syscall_dup3(oldfd, newfd, flags int32) int32 {
	if r, done := stdDupGate(oldfd, newfd, flags); done {
		return r
	}
	return rawDup3(oldfd, newfd, flags)
}

func rawDup2(oldfd, newfd int32) int32 {
	if oldfd == newfd {
		// dup2(fd, fd) returns fd when it is a valid descriptor; dup3 would EINVAL,
		// so validate with fcntl(F_GETFD) and return fd (musl's dup2 fallback).
		if _, _, e := syscall.Syscall(syscall.SYS_FCNTL, uintptr(oldfd),
			uintptr(syscall.F_GETFD), 0); e != 0 {
			return -int32(e)
		}
		return oldfd
	}
	return rawDup3(oldfd, newfd, 0)
}

func rawDup3(oldfd, newfd, flags int32) int32 {
	for {
		err := syscall.Dup3(int(oldfd), int(newfd), int(flags))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return newfd
	}
}
