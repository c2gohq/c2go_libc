// fdpath_darwin.go — #675: recover a directory path from an fd via
// fcntl(F_GETPATH) (darwin has no /proc). x/sys/unix has no pointer-arg
// fcntl wrapper, so this goes through unix.Syscall(SYS_FCNTL) — the
// supported libSystem-trampoline path.
//go:build darwin

package libc

import (
	"unsafe"

	"golang.org/x/sys/unix"
)

func fdDirPath(fd int32) string {
	var buf [1024]byte // MAXPATHLEN
	_, _, errno := unix.Syscall(unix.SYS_FCNTL, uintptr(fd),
		uintptr(unix.F_GETPATH), uintptr(unsafe.Pointer(&buf[0])))
	if errno != 0 {
		return ""
	}
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i])
		}
	}
	return ""
}
