// unistd_shim_unix.go — #675 stage C: thin unix syscall shims for the fd/path
// surface (readv/writev, the *at family, fchdir/fchown/chown/lchown, sync,
// gethostname) plus the conf bridges (getpagesize/sysconf/getentropy). Shim
// contract mirrors io.go: return the result or -errno; the C wrappers in
// source/unistd2.c apply musl's __syscall_ret shape. fds are REAL kernel fds
// (the unix fd layer is C-first, #607).

//go:build unix

package libc

import (
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

func errnoRet(err error) int64 {
	if err == nil {
		return 0
	}
	if eno, ok := err.(syscall.Errno); ok {
		return -int64(eno)
	}
	return -int64(syscall.EIO)
}

//go:linkname __c2go_syscall_readv
func __c2go_syscall_readv(fd int32, iov unsafe.Pointer, iovcnt int32) int64 {
	for {
		r, _, e := syscall.Syscall(syscall.SYS_READV, uintptr(fd), uintptr(iov), uintptr(iovcnt))
		if e == syscall.EINTR {
			continue // spurious runtime-preemption EINTR (see io.go header)
		}
		if e != 0 {
			return -int64(e)
		}
		return int64(r)
	}
}

//go:linkname __c2go_syscall_writev
func __c2go_syscall_writev(fd int32, iov unsafe.Pointer, iovcnt int32) int64 {
	for {
		r, _, e := syscall.Syscall(syscall.SYS_WRITEV, uintptr(fd), uintptr(iov), uintptr(iovcnt))
		if e == syscall.EINTR {
			continue
		}
		if e != 0 {
			return -int64(e)
		}
		return int64(r)
	}
}

// The *at family goes through golang.org/x/sys/unix — darwin's syscall
// package has neither the wrappers nor the SYS_ numbers (and raw traps are
// not the supported path there; unix.* routes through libSystem).

// fstatat completes the *at family (#675 C wave 2b). x/sys's Stat_t and the
// syscall package's are two generated mirrors of the SAME kernel struct
// (identical layout on both unix targets), so the pointer cast hands the
// x/sys result to the shared statFill marshal (stat.go / stat_<os>.go) that
// fills the caller's uniform c2go struct stat.
//go:linkname __c2go_syscall_fstatat
func __c2go_syscall_fstatat(dirfd int32, path *byte, buf unsafe.Pointer, flags int32) int64 {
	name := cstr(path)
	var ust unix.Stat_t
	for {
		err := unix.Fstatat(int(dirfd), name, &ust, int(flags))
		if err == unix.EINTR {
			continue
		}
		if err != nil {
			return errnoRet(err)
		}
		break
	}
	statFill((*cStat)(buf), (*syscall.Stat_t)(unsafe.Pointer(&ust)))
	return 0
}

//go:linkname __c2go_syscall_openat
func __c2go_syscall_openat(dirfd int32, path *byte, flags int32, mode uint32) int64 {
	fd, err := unix.Openat(int(dirfd), cstr(path), int(flags), mode)
	if err != nil {
		return errnoRet(err)
	}
	return int64(fd)
}

//go:linkname __c2go_syscall_mkdirat
func __c2go_syscall_mkdirat(dirfd int32, path *byte, mode uint32) int64 {
	return errnoRet(unix.Mkdirat(int(dirfd), cstr(path), mode))
}

//go:linkname __c2go_syscall_unlinkat
func __c2go_syscall_unlinkat(dirfd int32, path *byte, flags int32) int64 {
	return errnoRet(unix.Unlinkat(int(dirfd), cstr(path), int(flags)))
}

//go:linkname __c2go_syscall_renameat
func __c2go_syscall_renameat(fromfd int32, from *byte, tofd int32, to *byte) int64 {
	return errnoRet(unix.Renameat(int(fromfd), cstr(from), int(tofd), cstr(to)))
}

//go:linkname __c2go_syscall_fchdir
func __c2go_syscall_fchdir(fd int32) int64 {
	return errnoRet(syscall.Fchdir(int(fd)))
}

//go:linkname __c2go_syscall_fchown
func __c2go_syscall_fchown(fd int32, uid uint32, gid uint32) int64 {
	return errnoRet(syscall.Fchown(int(fd), int(int32(uid)), int(int32(gid))))
}

//go:linkname __c2go_syscall_chown
func __c2go_syscall_chown(path *byte, uid uint32, gid uint32, link int32) int64 {
	p := cstr(path)
	if link != 0 {
		return errnoRet(syscall.Lchown(p, int(int32(uid)), int(int32(gid))))
	}
	return errnoRet(syscall.Chown(p, int(int32(uid)), int(int32(gid))))
}

//go:linkname __c2go_syscall_sync
func __c2go_syscall_sync() int64 {
	syscall.Sync() // void on both OSes
	return 0
}

// gethostname/getentropy/getpagesize/sysconf moved to conf_bridge.go —
// cross-platform per the #675 "1+3" decision (source/conf.c wrappers).
