// pipe2_linux.go — #675: pipe2 has a real syscall on linux.
//go:build linux

package libc

import (
	"syscall"
	_ "unsafe"
)

//go:linkname __c2go_syscall_pipe2
func __c2go_syscall_pipe2(fds *[2]int32, flags int32) int64 {
	var p [2]int
	if err := syscall.Pipe2(p[:], int(flags)); err != nil {
		return errnoRet(err)
	}
	if err := c2goFDPair(&p); err != nil {
		return errnoRet(err)
	}
	fds[0], fds[1] = int32(p[0]), int32(p[1])
	return 0
}
