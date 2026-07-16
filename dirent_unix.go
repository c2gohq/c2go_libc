// dirent_unix.go — #678: the unix-only directory-fd bridges. On windows
// os.File.Fd() is a HANDLE, not a CRT fd, so a "directory fd" cannot be
// honestly delivered (#677) — dirfd/fdopendir stay absent there.
//go:build unix

package libc

import (
	"os"
	"syscall"
	_ "unsafe" // for //go:linkname
)

// __c2go_dirfd (#675): the kernel fd behind an open DIR stream (nftw's
// openat/fdopendir walk consumes it).
//go:linkname __c2go_dirfd
func __c2go_dirfd(handle int64) int64 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int64(syscall.EBADF)
	}
	return int64(ds.f.Fd())
}

// __c2go_fdopendir (#675): wrap an already-open directory fd (POSIX: the
// stream takes ownership). The path — needed only by rewinddir's reopen — is
// recovered per-OS (fdDirPath); when unrecoverable the stream still reads,
// only a later rewinddir fails.
//go:linkname __c2go_fdopendir
func __c2go_fdopendir(fd int32) int64 {
	path := fdDirPath(fd)
	f := os.NewFile(uintptr(fd), path)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	fi, err := f.Stat()
	if err != nil {
		return -int64(errnoOfPath(err))
	}
	if !fi.IsDir() {
		return -int64(syscall.ENOTDIR)
	}
	return int64(dirTab.alloc(&dirState{f: f, path: path}))
}
// sysInode: the real inode from unix stat (0 when unavailable).
func sysInode(fi os.FileInfo) uint64 {
	if st, ok := fi.Sys().(*syscall.Stat_t); ok {
		return st.Ino
	}
	return 0
}
