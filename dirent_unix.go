// dirent_unix.go — #678: the unix-only directory-fd bridges. On windows
// os.File.Fd() is a HANDLE, not a CRT fd, so a "directory fd" cannot be
// honestly delivered (#677) — dirfd/fdopendir stay absent there.
//go:build unix

package libc

import (
	"syscall"
	_ "unsafe" // for //go:linkname

	"github.com/c2gohq/c2go_libc/internal/posixdir"
)

// __c2go_dirfd (#675): the kernel fd behind an open DIR stream (nftw's
// openat/fdopendir walk consumes it).
//
//go:linkname __c2go_dirfd
func __c2go_dirfd(handle int64) int64 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int64(syscall.EBADF)
	}
	fd, err := ds.FD()
	if err != nil {
		return -int64(posixdir.Errno(err))
	}
	return int64(fd)
}

// __c2go_fdopendir (#675): wrap an already-open directory fd (POSIX: the
// stream takes ownership). The path — needed only by rewinddir's reopen — is
// recovered per-OS (fdDirPath); when unrecoverable the stream still reads,
// only a later rewinddir fails.
//
//go:linkname __c2go_fdopendir
func __c2go_fdopendir(fd int32) int64 {
	stream, err := posixdir.OpenFD(fd)
	if err != nil {
		return -int64(posixdir.Errno(err))
	}
	return int64(dirTab.alloc(stream))
}
