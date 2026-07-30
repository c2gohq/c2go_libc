// io.go — the Go syscall shims behind the POSIX fd layer (source/io_posix.c).
//
// The public fd surface — read/write/open/close/lseek/fcntl/dup2/dup3 and the
// path ops unlink/rmdir/rename — is C (io_posix.c, musl-shaped: a POSIX return
// value plus errno on failure). Each of those C wrappers composes exactly one
// __c2go_syscall_* shim here through a GoABI0 c2go_linkname. Only the syscall
// itself is Go: issuing a raw trap from C would bypass Go's syscall scheduler
// protocol, so the trap comes from Go's syscall package (the goroutine parks in
// the syscall and others run). Everything
// else — the POSIX shape, the errno convention, the variadic open/fcntl mode
// extraction — is C in io_posix.c, mirroring musl. This is the symmetric
// counterpart of the Windows fd layer, where source/io_windows.c instead calls
// the msvcrt CRT.
//
// Shim contract: return the syscall result on success, or -errno on failure (the
// raw Linux syscall convention). The C wrapper applies musl's __syscall_ret
// shape to it: `if (r < 0) { errno = -r; return -1; }`. So these shims never
// touch the C errno themselves.
//
// A blocking syscall under the Go runtime can be interrupted by the runtime's
// own async-preemption signal (SIGURG), surfacing a SPURIOUS EINTR that no
// application signal caused — which is why Go's os package wraps its syscalls in
// an EINTR retry (ignoringEINTR). Application signals here arrive via os/signal,
// not as EINTR to a raw syscall, so every EINTR these shims see is spurious:
// retry it, giving ported C the normal "the syscall completed" behaviour. (Close
// is the exception — see its note.)
//
//go:build unix

package libc

import (
	"syscall"
	"unsafe"
)

//go:linkname __c2go_syscall_read
func __c2go_syscall_read(fd int32, buf unsafe.Pointer, n uint64) int64 {
	if isStdFd(fd) {
		return __c2go_std_read(fd, buf, n) // virtualized: live os.Stdin/out/err
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	b := unsafe.Slice((*byte)(buf), n)
	for {
		m, err := syscall.Read(int(fd), b)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(m)
	}
}

//go:linkname __c2go_syscall_write
func __c2go_syscall_write(fd int32, buf unsafe.Pointer, n uint64) int64 {
	if isStdFd(fd) {
		return __c2go_std_write(fd, buf, n) // virtualized: live os.Stdin/out/err
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	b := unsafe.Slice((*byte)(buf), n)
	for {
		m, err := syscall.Write(int(fd), b)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(m)
	}
}

// __c2go_syscall_open takes the mode already extracted from the C vararg by the
// io_posix.c open() wrapper (0 when O_CREAT is absent); the C-side O_* flags are
// the host-native ones (bits/fcntl.h), so they pass straight to syscall.Open.
//
//go:linkname __c2go_syscall_open
func __c2go_syscall_open(path *byte, flags int32, mode uint32) int32 {
	name := cstr(path)
	for {
		fd, err := syscall.Open(name, int(flags), mode)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		fd, err = c2goFD(fd)
		if err != nil {
			return -errnoOf(err)
		}
		return int32(fd)
	}
}

//go:linkname __c2go_syscall_close
func __c2go_syscall_close(fd int32) int32 {
	// Do NOT retry close on EINTR: on Linux the descriptor is already closed, so
	// a retry could close an unrelated fd that was concurrently reused. POSIX
	// leaves the state unspecified; treating EINTR as success is the safe choice.
	err := syscall.Close(int(fd))
	if err != nil && err != syscall.EINTR {
		return -errnoOf(err)
	}
	return 0
}

//go:linkname __c2go_syscall_lseek
func __c2go_syscall_lseek(fd int32, off int64, whence int32) int64 {
	if isStdFd(fd) {
		return __c2go_std_seek(fd, off, whence)
	}
	for {
		n, err := syscall.Seek(int(fd), off, int(whence))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return n
	}
}

// __c2go_syscall_fcntl takes the optional int arg already extracted from the C
// vararg by the io_posix.c fcntl() wrapper (0 for commands that take none). The
// command codes and F_SETFL flag bits are host-native (see <fcntl.h>), so they
// pass straight through to the native fcntl.
//
//go:linkname __c2go_syscall_fcntl
func __c2go_syscall_fcntl(fd, cmd, arg int32) int32 {
	if isStdFd(fd) {
		// virtualized: run on the live object's real descriptor (Control access);
		// F_DUPFD hands back a raw snapshot fd, like dup(std).
		return c2goFcntlResult(cmd, stdFcntl(fd, cmd, arg))
	}
	return c2goFcntlResult(cmd, rawFcntl(fd, cmd, arg))
}

// rawFcntl is the un-routed fcntl body (also the Control-side worker for a
// virtualized std descriptor — see stdFcntl).
func rawFcntl(fd, cmd, arg int32) int32 {
	r, _, e := syscall.Syscall(syscall.SYS_FCNTL, uintptr(fd), uintptr(cmd), uintptr(arg))
	if e != 0 {
		return -int32(e)
	}
	return int32(r)
}

// Dup2 / Dup3 shims are per-OS (io_darwin.go / io_linux.go): Linux has the atomic
// dup3 syscall (and no dup2 on arm64), while macOS has only dup2, so dup3 there
// falls back to dup2 + fcntl(FD_CLOEXEC) — exactly musl's __dup3.

//go:linkname __c2go_syscall_unlink
func __c2go_syscall_unlink(path *byte) int32 {
	name := cstr(path)
	for {
		err := syscall.Unlink(name)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_rmdir
func __c2go_syscall_rmdir(path *byte) int32 {
	name := cstr(path)
	for {
		err := syscall.Rmdir(name)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_rename
func __c2go_syscall_rename(oldp, newp *byte) int32 {
	from, to := cstr(oldp), cstr(newp)
	for {
		err := syscall.Rename(from, to)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}
