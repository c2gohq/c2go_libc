// stdio_std_unix.go — unix std-descriptor operations beyond read/write/seek
// (stdio_std.go): the fd-layer entry points that the __c2go_syscall_* shims
// route here for fd 0/1/2. Wherever os.File has a native method (ReadAt/
// WriteAt/Sync/Truncate/Stat) we use it — poller-integrated and free of fd
// exposure. The rest (isatty/dup/fcntl/dup2-source) need the real descriptor:
// they reach it through SyscallConn().Control, which — unlike os.File.Fd() —
// has NO side effects (no blocking-mode flip, no poller deregistration), so a
// C isatty(1)/dup(1) cannot degrade a host-owned pollable *os.File.
//
//go:build unix

package libc

import (
	"syscall"
)

// stdControlFd runs fn on the live std object's real descriptor via
// SyscallConn().Control (side-effect-free access). fn returns the shim
// convention (result or -errno); conn failures map to -errno too.
func stdControlFd(which int32, fn func(fd int) int64) int64 {
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	sc, err := f.SyscallConn()
	if err != nil {
		return -stdErrno(err)
	}
	var r int64
	if cerr := sc.Control(func(fd uintptr) { r = fn(int(fd)) }); cerr != nil {
		return -stdErrno(cerr)
	}
	return r
}

// stdIsatty: identity-gated tty probe. After a host swap the stream is a
// pipe/file in every realistic case — report "not a tty" WITHOUT touching the
// host's object; on the startup object probe the real descriptor (rawIsatty,
// per-OS) under Control.
func stdIsatty(which int32) int32 {
	if __c2go_std_isdefault(which) == 0 {
		return 0
	}
	return int32(stdControlFd(which, func(fd int) int64 {
		return int64(rawIsatty(int32(fd)))
	}))
}

// stdDup: dup(std) snapshots the CURRENT target — a new kernel fd (>=3, 0/1/2
// are occupied) onto the same open file description. POSIX dup semantics;
// the snapshot does not follow later os.Std* swaps (it is a raw fd).
func stdDup(which int32) int32 {
	return int32(stdControlFd(which, func(fd int) int64 {
		nfd, err := syscall.Dup(fd)
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(nfd)
	}))
}

// stdFcntl: fcntl on the live descriptor (F_GETFL/F_SETFL/F_DUPFD/...).
// F_DUPFD naturally returns a raw snapshot fd like stdDup.
func stdFcntl(which, cmd, arg int32) int32 {
	return int32(stdControlFd(which, func(fd int) int64 {
		return int64(rawFcntl(int32(fd), cmd, arg))
	}))
}

// stdDupFrom: dup2/dup3 with a std SOURCE (dup2(1, n)) — duplicate the live
// target onto newfd. newfd is a raw fd (a std newfd is handled by the
// identity gate in the dup2/dup3 shims before reaching here).
func stdDupFrom(which, newfd, flags int32) int32 {
	return int32(stdControlFd(which, func(fd int) int64 {
		return int64(rawDup3(int32(fd), newfd, flags))
	}))
}

// stdFstat: fill C struct stat from the live object. os.File.Stat carries the
// real *syscall.Stat_t in Sys() — no descriptor access needed at all.
func stdFstat(which int32, buf *cStat) int32 {
	f := stdLive(which)
	if f == nil {
		return -int32(syscall.EBADF)
	}
	fi, err := f.Stat()
	if err != nil {
		return -int32(stdErrno(err))
	}
	st, ok := fi.Sys().(*syscall.Stat_t)
	if !ok {
		return -int32(syscall.EIO)
	}
	statFill(buf, st)
	return 0
}




// stdDupGate handles the std-descriptor cases of dup2/dup3(oldfd, newfd);
// (result, true) when it fully handled the call:
//   - std NEWFD (the C redirect idiom dup2(x, 1)): allowed only while that
//     stream is the STARTUP object — its kernel fd IS the literal number, so
//     the plain kernel dup2/dup3 does the POSIX-faithful redirect and os.Std*
//     (wrapping that fd) follows. After a host swap, repointing the fd under a
//     host-owned *os.File would desync its runtime state → EINVAL.
//   - std OLDFD (dup2(1, n)): duplicate the LIVE target onto newfd via
//     Control access. (dup2(std, same-std) returns the fd — valid by
//     construction; dup3's same-fd EINVAL nuance is not distinguished here.)
func stdDupGate(oldfd, newfd, flags int32) (int32, bool) {
	if isStdFd(newfd) && __c2go_std_isdefault(newfd) == 0 {
		return -int32(syscall.EINVAL), true
	}
	if isStdFd(oldfd) {
		if oldfd == newfd {
			return newfd, true
		}
		return stdDupFrom(oldfd, newfd, flags), true
	}
	return 0, false
}
