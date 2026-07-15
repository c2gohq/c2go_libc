// stat_windows.go — the Windows file-metadata bridges (source/stat_windows.c),
// the per-OS counterpart of the unix stat.go. Go-first per the io_windows.go
// rule: stat/lstat/chmod/mkdir/access take a PATH, so the os package serves
// them directly (no msvcrt _stat64); fstat is fd-bound, so the C wrapper
// recovers the WIN32 HANDLE via _get_osfhandle and the bridge fills from
// GetFileInformationByHandle. All fill the UNIFORM cStat (stat_types.go) —
// never a per-OS layout copy.
//
// Field fidelity on Windows: ino/dev come from the file index / volume serial
// where a handle is available (fstat), 0 otherwise (MinGW _stat is no better);
// nlink 1; uid/gid 0; blksize 4096; blocks derived from size. st_mode maps
// FileMode → S_IFDIR/S_IFREG/S_IFLNK plus 0666/0444 (read-only attribute) and
// 0111 for directories, mirroring the MinGW _stat mode shape.
//
// Shim contract mirrors stat.go: 0 on success or -errno (MinGW value via
// winErrno) on failure.
//
//go:build windows

package libc

import (
	"os"
	"syscall"
	"unsafe"
)

const (
	winS_IFDIR = 0040000
	winS_IFREG = 0100000
	winS_IFLNK = 0120000
	winS_IFIFO = 0010000
	winS_IFCHR = 0020000
)

// winMode maps an os.FileMode to the uniform st_mode bits.
func winMode(m os.FileMode) uint32 {
	var mode uint32
	switch {
	case m&os.ModeSymlink != 0:
		mode = winS_IFLNK
	case m.IsDir():
		mode = winS_IFDIR
	case m&os.ModeNamedPipe != 0:
		mode = winS_IFIFO
	case m&os.ModeCharDevice != 0:
		mode = winS_IFCHR
	default:
		mode = winS_IFREG
	}
	// Permission shape mirrors MinGW _stat: rw (or r for read-only) replicated
	// user/group/other, +x for directories.
	if m.Perm()&0200 != 0 {
		mode |= 0666
	} else {
		mode |= 0444
	}
	if m.IsDir() {
		mode |= 0111
	}
	return mode
}

func filetimeTimespec(ft syscall.Filetime) cTimespec {
	ns := ft.Nanoseconds()
	return cTimespec{ns / 1e9, ns % 1e9}
}

// winStatFill fills the uniform cStat from an os.FileInfo. Timestamps come
// from the Win32FileAttributeData behind Sys() when present (atime + ctime as
// creation time — the closest Windows analogue), else all three fall back to
// ModTime.
func winStatFill(dst *cStat, fi os.FileInfo) {
	*dst = cStat{}
	dst.nlink = 1
	dst.mode = winMode(fi.Mode())
	dst.size = fi.Size()
	dst.blksize = 4096
	dst.blocks = (fi.Size() + 511) / 512
	if d, ok := fi.Sys().(*syscall.Win32FileAttributeData); ok && d != nil {
		dst.atim = filetimeTimespec(d.LastAccessTime)
		dst.mtim = filetimeTimespec(d.LastWriteTime)
		dst.ctim = filetimeTimespec(d.CreationTime)
	} else {
		ns := fi.ModTime().UnixNano()
		ts := cTimespec{ns / 1e9, ns % 1e9}
		dst.atim, dst.mtim, dst.ctim = ts, ts, ts
	}
}

//go:linkname __c2go_syscall_stat
func __c2go_syscall_stat(path *byte, buf unsafe.Pointer) int32 {
	fi, err := os.Stat(cstr(path))
	if err != nil {
		return -winErrno(err)
	}
	winStatFill((*cStat)(buf), fi)
	return 0
}

//go:linkname __c2go_syscall_lstat
func __c2go_syscall_lstat(path *byte, buf unsafe.Pointer) int32 {
	fi, err := os.Lstat(cstr(path))
	if err != nil {
		return -winErrno(err)
	}
	winStatFill((*cStat)(buf), fi)
	return 0
}

// __c2go_syscall_fstat_h fills cStat for an open HANDLE (the C fstat wrapper
// recovers it from a CRT fd via _get_osfhandle). GetFileInformationByHandle
// also yields the volume serial + file index — the closest st_dev/st_ino.
//
//go:linkname __c2go_syscall_fstat_h
func __c2go_syscall_fstat_h(h int64, buf unsafe.Pointer) int32 {
	var d syscall.ByHandleFileInformation
	if err := syscall.GetFileInformationByHandle(syscall.Handle(h), &d); err != nil {
		return -winErrno(err)
	}
	dst := (*cStat)(buf)
	*dst = cStat{}
	dst.dev = uint64(d.VolumeSerialNumber)
	dst.ino = uint64(d.FileIndexHigh)<<32 | uint64(d.FileIndexLow)
	dst.nlink = uint64(d.NumberOfLinks)
	var m uint32 = winS_IFREG | 0666
	if d.FileAttributes&syscall.FILE_ATTRIBUTE_DIRECTORY != 0 {
		m = winS_IFDIR | 0777
	} else if d.FileAttributes&syscall.FILE_ATTRIBUTE_READONLY != 0 {
		m = winS_IFREG | 0444
	}
	dst.mode = m
	dst.size = int64(d.FileSizeHigh)<<32 | int64(d.FileSizeLow)
	dst.blksize = 4096
	dst.blocks = (dst.size + 511) / 512
	dst.atim = filetimeTimespec(d.LastAccessTime)
	dst.mtim = filetimeTimespec(d.LastWriteTime)
	dst.ctim = filetimeTimespec(d.CreationTime)
	return 0
}

// stdFstatWin: fstat on a virtualized std descriptor — fill from the live
// os.Std* object's Stat() (console handles may refuse; map the error).
//
//go:linkname __c2go_std_fstat_win
func __c2go_std_fstat_win(which int32, buf unsafe.Pointer) int32 {
	f := stdLive(which)
	if f == nil {
		return -errEBADF
	}
	fi, err := f.Stat()
	if err != nil {
		return -winErrno(err)
	}
	winStatFill((*cStat)(buf), fi)
	return 0
}

//go:linkname __c2go_syscall_chmod
func __c2go_syscall_chmod(path *byte, mode uint32) int32 {
	if err := os.Chmod(cstr(path), os.FileMode(mode&0777)); err != nil {
		return -winErrno(err)
	}
	return 0
}

//go:linkname __c2go_syscall_mkdir
func __c2go_syscall_mkdir(path *byte, mode uint32) int32 {
	if err := os.Mkdir(cstr(path), os.FileMode(mode&0777)); err != nil {
		return -winErrno(err)
	}
	return 0
}

// access: F_OK/R_OK are existence (a stat-able file is readable on Windows);
// W_OK checks the read-only attribute; X_OK is treated like R_OK (the CRT
// _access does the same — Windows has no execute bit).
//
//go:linkname __c2go_syscall_access
func __c2go_syscall_access(path *byte, amode int32) int32 {
	fi, err := os.Stat(cstr(path))
	if err != nil {
		return -winErrno(err)
	}
	const wok = 2 // W_OK
	if amode&wok != 0 && fi.Mode().Perm()&0200 == 0 {
		return -errEACCES
	}
	return 0
}
