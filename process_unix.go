// process_unix.go — Unix-only process-id bridges.
//
// getppid and the real/effective uid/gid queries have no cross-platform os-package
// wrapper (os only exposes Getpid) and no MinGW/Windows equivalent, so they bridge
// straight to package syscall here under a unix build tag. getpid itself is
// cross-platform and lives in process.go. Return types mirror the C types:
// getppid is pid_t (int32), the uid/gid family is uid_t/gid_t (uint32).
//go:build unix

package libc

import (
	"syscall"
	_ "unsafe" // for //go:linkname
)

//go:linkname Getppid
func Getppid() int32 { return int32(syscall.Getppid()) }

//go:linkname Getuid
func Getuid() uint32 { return uint32(syscall.Getuid()) }

//go:linkname Geteuid
func Geteuid() uint32 { return uint32(syscall.Geteuid()) }

//go:linkname Getgid
func Getgid() uint32 { return uint32(syscall.Getgid()) }

//go:linkname Getegid
func Getegid() uint32 { return uint32(syscall.Getegid()) }

// Kill implements kill(pid, sig) (#675). kill(pid, 0) is the existence probe;
// in-process self-signalling should use raise() (the Go-side dispatcher), so
// this bridge matters for real cross-process signals and probes.
//go:linkname Kill
func Kill(pid int32, sig int32) int32 {
	if err := syscall.Kill(int(pid), syscall.Signal(sig)); err != nil {
		if eno, ok := err.(syscall.Errno); ok {
			setErrno(int32(eno))
		}
		return -1
	}
	return 0
}
