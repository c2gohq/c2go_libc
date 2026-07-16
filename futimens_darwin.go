// futimens_darwin.go — darwin has a futimens syscall but neither the
// syscall package nor x/sys exposes it (no wrapper, no SYS_ number), and
// the Go utimensat wrapper cannot take a NULL path. Recover the fd's path
// via fcntl(F_GETPATH) and go through UtimesNanoAt — the fdopendir/
// rewinddir precedent (#675 C wave 2a). Same caveat as there: a concurrent
// rename between F_GETPATH and the utimensat re-resolution retargets the
// new occupant of the old path (documented race, accepted).

//go:build darwin

package libc

import (
	"unsafe"

	"golang.org/x/sys/unix"
)

//go:linkname __c2go_syscall_futimens
func __c2go_syscall_futimens(fd int32, times unsafe.Pointer) int64 {
	path := fdDirPath(fd) // F_GETPATH works for any fd kind, not only dirs
	if path == "" {
		return -int64(unix.EBADF)
	}
	ts := timespecPair(times)
	for {
		err := unix.UtimesNanoAt(unix.AT_FDCWD, path, ts, 0)
		if err == unix.EINTR {
			continue
		}
		if err != nil {
			return errnoRet(err)
		}
		return 0
	}
}
