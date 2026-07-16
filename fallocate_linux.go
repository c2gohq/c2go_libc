// fallocate_linux.go — posix_fallocate's kernel half: fallocate(fd, 0,
// off, len). linux-only like musl's source (darwin ships no
// posix_fallocate at all — the C symbol is absent there, no fake stub).

//go:build linux

package libc

import (
	_ "unsafe" // for //go:linkname

	"golang.org/x/sys/unix"
)

//go:linkname __c2go_syscall_fallocate
func __c2go_syscall_fallocate(fd int32, off int64, length int64) int64 {
	for {
		err := unix.Fallocate(int(fd), 0, off, length)
		if err == unix.EINTR {
			continue
		}
		if err != nil {
			return errnoRet(err)
		}
		return 0
	}
}
