// pipe2_darwin.go — #675: darwin has no pipe2 syscall; compose pipe +
// per-fd CLOEXEC/NONBLOCK, matching what libSystem's callers do by hand.
//go:build darwin

package libc

import (
	"syscall"
	_ "unsafe"
)

//go:linkname __c2go_syscall_pipe2
func __c2go_syscall_pipe2(fds *[2]int32, flags int32) int64 {
	var p [2]int
	if err := syscall.Pipe(p[:]); err != nil {
		return errnoRet(err)
	}
	const oCLOEXEC, oNONBLOCK = 0x1000000, 0x4 // darwin O_CLOEXEC / O_NONBLOCK
	for _, fd := range p {
		if flags&oCLOEXEC != 0 {
			syscall.CloseOnExec(fd)
		}
		if flags&oNONBLOCK != 0 {
			if err := syscall.SetNonblock(fd, true); err != nil {
				syscall.Close(p[0])
				syscall.Close(p[1])
				return errnoRet(err)
			}
		}
	}
	fds[0], fds[1] = int32(p[0]), int32(p[1])
	return 0
}
