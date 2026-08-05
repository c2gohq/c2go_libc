// SPDX-License-Identifier: AGPL-3.0-only

//go:build darwin

package posixdir

import (
	"unsafe"

	"golang.org/x/sys/unix"
)

func fdPath(fd int32) string {
	var buffer [1024]byte
	_, _, errno := unix.Syscall(unix.SYS_FCNTL, uintptr(fd),
		uintptr(unix.F_GETPATH), uintptr(unsafe.Pointer(&buffer[0])))
	if errno != 0 {
		return ""
	}
	for index, value := range buffer {
		if value == 0 {
			return string(buffer[:index])
		}
	}
	return ""
}
