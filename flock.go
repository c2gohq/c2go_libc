// flock.go — the Go bridges behind the file-locking layer (#648): fcntl
// record locks (F_GETLK/F_SETLK/F_SETLKW, source/io_posix.c) and BSD flock()
// (source/fsops_posix.c).
//
// struct flock is a UNIFORM c2go layout and the command / l_type codes are
// c2go-uniform too (<fcntl.h>): the native values DIFFER per OS (linux
// F_SETLK=6 vs darwin=8; darwin F_WRLCK=3 vs linux=1), so this bridge
// translates both directions — commands/types to the host's syscall
// constants on the way in, the host's l_type back to uniform on the way out
// (F_GETLK) — and copies field-by-field into the host's syscall.Flock_t,
// whose member ORDER differs between linux and darwin (the cStat pattern;
// never a raw-layout pass-through).
//
// Semantics reminder (tested accordingly): fcntl record locks are
// PER-PROCESS (two fds of one process do not conflict; locks merge), while
// flock locks the OPEN FILE DESCRIPTION (two open()s conflict even within
// one process). Virtualized std descriptors route through SyscallConn
// Control like the other destructive fd ops.
//
//go:build unix

package libc

import (
	"syscall"
	"unsafe"

	_ "unsafe" // for //go:linkname
)

// cFlock mirrors <fcntl.h>'s uniform struct flock exactly:
// short/short/(hole)/i64/i64/i32 → 32 bytes.
type cFlock struct {
	typ    int16 // l_type   @0 (uniform F_RDLCK 0 / F_WRLCK 1 / F_UNLCK 2)
	whence int16 // l_whence @2
	_pad0  int32 // hole before the 8-aligned l_start
	start  int64 // l_start  @8
	len    int64 // l_len    @16
	pid    int32 // l_pid    @24
	_pad1  int32 // tail pad @28
} // 32 bytes

// Uniform codes (must match <fcntl.h>).
const (
	cF_GETLK  = 0x1001
	cF_SETLK  = 0x1002
	cF_SETLKW = 0x1003

	cF_RDLCK = 0
	cF_WRLCK = 1
	cF_UNLCK = 2
)

// typeToHost / typeFromHost translate the uniform l_type ↔ the host's
// syscall constants (per-OS values via the syscall package — no hardcoding).
func typeToHost(t int16) (int16, bool) {
	switch t {
	case cF_RDLCK:
		return int16(syscall.F_RDLCK), true
	case cF_WRLCK:
		return int16(syscall.F_WRLCK), true
	case cF_UNLCK:
		return int16(syscall.F_UNLCK), true
	}
	return 0, false
}

func typeFromHost(t int16) int16 {
	switch t {
	case int16(syscall.F_RDLCK):
		return cF_RDLCK
	case int16(syscall.F_WRLCK):
		return cF_WRLCK
	default:
		return cF_UNLCK
	}
}

//go:linkname __c2go_fcntl_flock
func __c2go_fcntl_flock(fd, cmd int32, lk unsafe.Pointer) int32 {
	if lk == nil {
		return -int32(syscall.EINVAL)
	}
	c := (*cFlock)(lk)
	hostType, ok := typeToHost(c.typ)
	if !ok {
		return -int32(syscall.EINVAL)
	}
	var hostCmd int
	switch cmd {
	case cF_GETLK:
		hostCmd = syscall.F_GETLK
	case cF_SETLK:
		hostCmd = syscall.F_SETLK
	case cF_SETLKW:
		hostCmd = syscall.F_SETLKW
	default:
		return -int32(syscall.EINVAL)
	}
	var h syscall.Flock_t
	h.Type = hostType
	h.Whence = c.whence
	h.Start = c.start
	h.Len = c.len
	do := func(realfd int) int64 {
		for {
			err := syscall.FcntlFlock(uintptr(realfd), hostCmd, &h)
			if err == syscall.EINTR && cmd == cF_SETLKW {
				continue // only the blocking wait retries; SETLK/GETLK never EINTR
			}
			if err != nil {
				return -int64(errnoOf(err))
			}
			return 0
		}
	}
	var r int64
	if isStdFd(fd) {
		r = stdControlFd(fd, do)
	} else {
		r = do(int(fd))
	}
	if r < 0 {
		return int32(r)
	}
	if cmd == cF_GETLK {
		// Translate the host's answer back to the uniform layout. An
		// unconflicted probe reports F_UNLCK with the rest unchanged (POSIX).
		c.typ = typeFromHost(h.Type)
		c.whence = h.Whence
		c.start = h.Start
		c.len = h.Len
		c.pid = h.Pid
	}
	return 0
}

//go:linkname __c2go_syscall_flock
func __c2go_syscall_flock(fd, op int32) int32 {
	do := func(realfd int) int64 {
		for {
			err := syscall.Flock(realfd, int(op))
			if err == syscall.EINTR {
				continue
			}
			if err != nil {
				return -int64(errnoOf(err))
			}
			return 0
		}
	}
	var r int64
	if isStdFd(fd) {
		r = stdControlFd(fd, do)
	} else {
		r = do(int(fd))
	}
	return int32(r)
}
