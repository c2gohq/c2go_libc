// io_darwin.go — the dup2/dup3 syscall shims on macOS (behind io_posix.c's
// dup2/dup3 wrappers). Darwin has no dup3 syscall, so __c2go_syscall_dup3 is
// musl's fallback: dup2 + fcntl(F_SETFD, FD_CLOEXEC) when O_CLOEXEC is requested
// (musl src/unistd/dup3.c). __c2go_syscall_dup2 is the native darwin dup2.
//
// Both return the new fd on success or -errno on failure (the shim contract in
// io.go); the C wrapper turns -errno into errno + -1.
//
//go:build darwin

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
	for {
		err := syscall.Dup2(int(oldfd), int(newfd))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return newfd
	}
}

func rawDup3(oldfd, newfd, flags int32) int32 {
	if r := rawDup2(oldfd, newfd); r < 0 {
		return r // errno already encoded by the dup2 body
	}
	// No atomic dup3 on darwin: apply close-on-exec separately (musl's fallback,
	// which likewise ignores the fcntl result).
	if flags&int32(syscall.O_CLOEXEC) != 0 {
		syscall.Syscall(syscall.SYS_FCNTL, uintptr(newfd),
			uintptr(syscall.F_SETFD), uintptr(syscall.FD_CLOEXEC))
	}
	return newfd
}
