// dirent.go — unmanaged Go bridges for directory streams (csrc/dirent.c).
// Root libc stores a generation-stamped ID in DIR and resolves it through the
// handle table. internal/posixdir owns the stream behavior shared with mlib's
// direct managed-pointer carrier.
// dirfd/fdopendir are unix-only (dirent_unix.go): windows' os.File.Fd() is a
// HANDLE, not a CRT fd (#677).

package libc

import (
	"syscall"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixdir"
)

// cDirent is retained as the root package's C-layout test/view type. The
// shared core fills the layout-compatible posixdir.Dirent representation.
type cDirent struct {
	ino    uint64
	off    int64
	reclen uint16
	typ    uint8
	name   [256]byte
}

var dirTab handleTable[posixdir.Stream]

//go:linkname __c2go_opendir
func __c2go_opendir(path *byte) int64 {
	name := cstr(path)
	stream, err := posixdir.Open(name)
	if err != nil {
		return -int64(posixdir.Errno(err))
	}
	return int64(dirTab.alloc(stream))
}

//go:linkname __c2go_readdir
func __c2go_readdir(handle int64, de unsafe.Pointer) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	ok, err := ds.Read((*posixdir.Dirent)(de))
	if err != nil {
		return -posixdir.Errno(err)
	}
	if !ok {
		return 0
	}
	return 1
}

//go:linkname __c2go_closedir
func __c2go_closedir(handle int64) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	err := ds.Close()
	dirTab.free(uint64(handle))
	if err != nil {
		return -posixdir.Errno(err)
	}
	return 0
}

//go:linkname __c2go_rewinddir
func __c2go_rewinddir(handle int64) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	if err := ds.Rewind(); err != nil {
		return -posixdir.Errno(err)
	}
	return 0
}
