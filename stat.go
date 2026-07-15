// stat.go — the Go bridges behind the POSIX file-metadata layer
// (source/stat_posix.c). Same split as the fd layer (io.go): the public surface —
// stat/lstat/fstat/chmod/fchmod/mkdir/access/umask — is C (musl-shaped, errno on
// failure), and only the syscall itself is Go, because c2go-compiled C cannot
// issue a raw syscall.
//
// Shim contract (identical to io.go): return the syscall result on success, or
// -errno on failure; the C wrapper turns -errno into errno + -1. EINTR is retried
// (spurious under the Go runtime — see io.go).
//
// struct stat is a UNIFORM c2go layout (see include/sys/stat.h): cStat below
// MIRRORS it exactly (all fixed-width, identical on every target — dev/ino/nlink
// u64, mode/uid/gid u32 with a 4-byte hole before the 8-aligned rdev, size/
// blksize/blocks i64, three {i64,i64} timespecs; 120 bytes). The stat trio fills
// the caller's buffer field-by-field from the host syscall.Stat_t via the per-OS
// statFill (stat_darwin.go / stat_linux.go) — never a memcpy of a per-OS layout.
// The buffer is written only AFTER the blocking syscall, so the *cStat pointer
// parameter (tracked, copystack-adjusted) stays valid; nothing is laundered
// through uintptr.
//
//go:build unix

package libc

import (
	"syscall"
	"unsafe"
)

// cTimespec / cStat (the uniform struct stat mirror) live in stat_types.go —
// portable, shared with the Windows bridges (stat_windows.go).

//go:linkname __c2go_syscall_stat
func __c2go_syscall_stat(path *byte, buf unsafe.Pointer) int32 {
	name := cstr(path)
	var st syscall.Stat_t
	for {
		err := syscall.Stat(name, &st)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		break
	}
	statFill((*cStat)(buf), &st)
	return 0
}

//go:linkname __c2go_syscall_lstat
func __c2go_syscall_lstat(path *byte, buf unsafe.Pointer) int32 {
	name := cstr(path)
	var st syscall.Stat_t
	for {
		err := syscall.Lstat(name, &st)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		break
	}
	statFill((*cStat)(buf), &st)
	return 0
}

//go:linkname __c2go_syscall_fstat
func __c2go_syscall_fstat(fd int32, buf unsafe.Pointer) int32 {
	if isStdFd(fd) {
		return stdFstat(fd, (*cStat)(buf)) // virtualized: live os.Std*.Stat()
	}
	var st syscall.Stat_t
	for {
		err := syscall.Fstat(int(fd), &st)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		break
	}
	statFill((*cStat)(buf), &st)
	return 0
}

//go:linkname __c2go_syscall_chmod
func __c2go_syscall_chmod(path *byte, mode uint32) int32 {
	name := cstr(path)
	for {
		err := syscall.Chmod(name, mode)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_fchmod
func __c2go_syscall_fchmod(fd int32, mode uint32) int32 {
	if isStdFd(fd) { // #658 M1: the last fd-layer entry that bypassed the virtual std route
		return int32(stdControlFd(fd, func(real int) int64 {
			for {
				err := syscall.Fchmod(real, mode)
				if err == syscall.EINTR {
					continue
				}
				if err != nil {
					return -int64(errnoOf(err))
				}
				return 0
			}
		}))
	}
	for {
		err := syscall.Fchmod(int(fd), mode)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_mkdir
func __c2go_syscall_mkdir(path *byte, mode uint32) int32 {
	name := cstr(path)
	for {
		err := syscall.Mkdir(name, mode)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_access
func __c2go_syscall_access(path *byte, amode int32) int32 {
	name := cstr(path)
	for {
		err := syscall.Access(name, uint32(amode))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

// umask never fails; syscall.Umask returns the previous mask.
//go:linkname __c2go_syscall_umask
func __c2go_syscall_umask(mask uint32) uint32 {
	return uint32(syscall.Umask(int(mask)))
}
