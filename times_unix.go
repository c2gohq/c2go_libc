// times_unix.go — time/stat thin-wrapper shims: utimensat + mkfifo (the
// per-OS futimens halves live in futimens_{linux,darwin}.go, and the
// linux-only fallocate in fallocate_linux.go). Shim contract mirrors io.go:
// result or -errno; C wrappers in source/stat2.c apply __syscall_ret.

//go:build unix

package libc

import (
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

// cTimespec2 mirrors the C `struct timespec [2]` argument (two {i64,i64}
// pairs — identical to unix.Timespec on both targets, so the values pass
// through unconverted; the UTIME_NOW/UTIME_OMIT sentinels are the per-OS
// native ones straight from <sys/stat.h>).
func timespecPair(times unsafe.Pointer) []unix.Timespec {
	if times == nil {
		return nil // both timestamps = now (kernel semantics)
	}
	return (*[2]unix.Timespec)(times)[:]
}

//go:linkname __c2go_syscall_utimensat
func __c2go_syscall_utimensat(dirfd int32, path *byte, times unsafe.Pointer, flags int32) int64 {
	name := cstr(path)
	ts := timespecPair(times)
	for {
		err := unix.UtimesNanoAt(int(dirfd), name, ts, int(flags))
		if err == unix.EINTR {
			continue
		}
		if err != nil {
			return errnoRet(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_mkfifo
func __c2go_syscall_mkfifo(path *byte, mode uint32) int64 {
	name := cstr(path)
	for {
		err := syscall.Mkfifo(name, mode)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return errnoRet(err)
		}
		return 0
	}
}
