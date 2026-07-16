// dirent.go — Go bridges for directory streams (source/dirent.c). opendir stores
// an *os.File in the shared handle table and hands its id back as the DIR handle;
// readdir pulls one os.DirEntry at a time (batched) and fills the C struct dirent
// through an unsafe.Pointer cast — the same shape statFill uses for cStat. Cross-
// platform since #678 (os.File.ReadDir is the enumeration primitive).
// dirfd/fdopendir are unix-only (dirent_unix.go): windows' os.File.Fd() is a
// HANDLE, not a CRT fd (#677).

package libc

import (
	"errors"
	"io"
	"os"
	"syscall"
	"unsafe"
)

// cDirent mirrors the C `struct dirent` (dirent.h) byte-for-byte:
//   ino_t d_ino @0, off_t d_off @8, unsigned short d_reclen @16,
//   unsigned char d_type @18, char d_name[256] @19  (size 280, 8-aligned).
type cDirent struct {
	ino    uint64
	off    int64
	reclen uint16
	typ    uint8
	name   [256]byte
}

// dirState is one open directory stream: the *os.File, the path (rewinddir
// reopens — Go's cached read state cannot be rewound by a bare Seek), a batch of
// not-yet-returned entries, and a synthetic d_ino/d_off counter (nonzero).
type dirState struct {
	f       *os.File
	path    string
	pending []os.DirEntry
	off     int64
	dot     uint8 // 0/1: still owe "."/".." (os.ReadDir omits them; POSIX readdir yields them)
}

var dirTab handleTable[dirState]

// errnoOfPath extracts the errno from an *os.PathError (or any wrapped
// syscall.Errno); errnoOf only unwraps a bare syscall.Errno.
func errnoOfPath(err error) int32 {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		return int32(errno)
	}
	return int32(syscall.EIO)
}

//go:linkname __c2go_opendir
func __c2go_opendir(path *byte) int64 {
	name := cstr(path)
	f, err := os.Open(name)
	if err != nil {
		return -int64(errnoOfPath(err))
	}
	fi, err := f.Stat()
	if err != nil {
		f.Close()
		return -int64(errnoOfPath(err))
	}
	if !fi.IsDir() {
		f.Close()
		return -int64(syscall.ENOTDIR)
	}
	return int64(dirTab.alloc(&dirState{f: f, path: name}))
}

//go:linkname __c2go_readdir
func __c2go_readdir(handle int64, de unsafe.Pointer) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	// os.ReadDir drops "." and ".."; POSIX readdir yields them first.
	if ds.dot < 2 {
		name := "."
		if ds.dot == 1 {
			name = ".."
		}
		ds.dot++
		ds.off++
		fillDirentRaw((*cDirent)(de), name, 4 /*DT_DIR*/, uint64(ds.off))
		return 1
	}
	if len(ds.pending) == 0 {
		ents, err := ds.f.ReadDir(64)
		if len(ents) == 0 {
			if err == nil || err == io.EOF {
				return 0 // end of directory
			}
			return -errnoOfPath(err)
		}
		ds.pending = ents // an io.EOF alongside entries is re-detected next call
	}
	e := ds.pending[0]
	ds.pending = ds.pending[1:]
	ds.off++
	fillDirent((*cDirent)(de), e, uint64(ds.off))
	return 1
}

//go:linkname __c2go_closedir
func __c2go_closedir(handle int64) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	err := ds.f.Close()
	dirTab.free(uint64(handle))
	if err != nil {
		return -errnoOfPath(err)
	}
	return 0
}


//go:linkname __c2go_rewinddir
func __c2go_rewinddir(handle int64) int32 {
	ds := dirTab.get(uint64(handle))
	if ds == nil {
		return -int32(syscall.EBADF)
	}
	nf, err := os.Open(ds.path)
	if err != nil {
		return -errnoOfPath(err)
	}
	ds.f.Close()
	ds.f = nf
	ds.pending = nil
	ds.off = 0
	ds.dot = 0
	return 0
}

func fillDirent(de *cDirent, e os.DirEntry, seq uint64) {
	// #661: d_ino carries the REAL inode when the platform exposes it (unix
	// Info().Sys() is a *syscall.Stat_t) — C code deduplicating by inode (fts
	// walkers) relies on it. The synthetic sequence number remains d_off and
	// the fallback ino (Windows / Info error).
	ino := seq
	if fi, err := e.Info(); err == nil {
		if real := sysInode(fi); real != 0 {
			ino = real // per-OS: unix Stat_t.Ino; windows has none (0 -> seq)
		}
	}
	fillDirentRaw(de, e.Name(), direntType(e.Type()), ino)
	de.off = int64(seq) // readdir position, not the inode
}

func fillDirentRaw(de *cDirent, name string, typ uint8, ino uint64) {
	de.ino = ino
	de.off = int64(ino)
	de.typ = typ
	if len(name) > 255 {
		name = name[:255]
	}
	copy(de.name[:], name)
	de.name[len(name)] = 0
	de.reclen = uint16(19 + len(name) + 1)
}

func direntType(m os.FileMode) uint8 {
	switch {
	case m&os.ModeDir != 0:
		return 4 // DT_DIR
	case m&os.ModeSymlink != 0:
		return 10 // DT_LNK
	case m&os.ModeNamedPipe != 0:
		return 1 // DT_FIFO
	case m&os.ModeSocket != 0:
		return 12 // DT_SOCK
	case m&os.ModeCharDevice != 0:
		return 2 // DT_CHR
	case m&os.ModeDevice != 0:
		return 6 // DT_BLK
	case m.IsRegular():
		return 8 // DT_REG
	default:
		return 0 // DT_UNKNOWN
	}
}
