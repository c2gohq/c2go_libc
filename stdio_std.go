// stdio_std.go — the Go side of the VIRTUALIZED std descriptors 0/1/2.
//
// In the C world, fd numbers 0/1/2 denote Go's live os.Stdin / os.Stdout /
// os.Stderr; fds >= 3 are kernel fds as usual. EVERY fd-layer entry point
// (read/write/lseek/fcntl/dup/dup2/isatty/fstat/... — the unix __c2go_syscall_*
// shims and the Windows CRT wrappers in io_windows.c) routes fd<=2 through the
// shims here, which re-read the os package VARIABLE per operation — the single
// source of truth for where the process's std streams point, with no libc-side
// cache (the timezone doctrine). Because the routing happens at the fd layer,
// the kernel fd number never leaks into C: fileno(stdout) is literally 1, and
// write(1, ...), fdopen(1, ...), fprintf(stdout, ...) all resolve to the same
// live sink — POSIX descriptor identity holds INSIDE the C world.
//
// Go host redirection (os.Stdout = w) is therefore visible to the very next C
// operation; C-side redirection (freopen/dup2 onto 0/1/2) stays kernel-level
// and is identity-gated: allowed only while os.Std* still IS the startup
// object (stdInit), refused (EINVAL, without killing the stream) after a host
// swap — repointing the kernel fd under a host-owned *os.File would desync its
// runtime state. The identity anchor is a policy gate, NOT a sink cache: data
// routing always reads the live variable.
//
// Concurrency contract (document loudest): reassigning os.Std* is a QUIESCENT
// operation — doing it concurrently with C stdio is a Go data race (the same
// race class as fmt.Println vs a swap) and adjacent operations may route to
// different sinks. Cross-world write ordering is serialized by the os.File's
// own internal lock, one level below C's stdio buffer.
//
// Shim contract mirrors io.go: the operation's result, or -errno on failure.
// Portable (Windows included) — Windows std I/O no longer depends on CRT fds
// 0/1/2 for console handling.

package libc

import (
	"errors"
	"io"
	"os"
	"syscall"
	"unsafe"
)

// stdInit anchors the three STARTUP std objects for identity comparison (only):
// os.Std* == stdInit[i] means "the host has not replaced this stream", which
// gates the tty probe and kernel-level redirects. Captured at package init —
// c2go-libc is a dependency of any user of it, so this init runs before any
// host code can reassign os.Std*.
var stdInit = [3]*os.File{os.Stdin, os.Stdout, os.Stderr}

// stdLive returns the live os.Std* for descriptor 0/1/2, nil-guarded (a
// pathological host may set one to nil).
func stdLive(which int32) *os.File {
	switch which {
	case 0:
		return os.Stdin
	case 1:
		return os.Stdout
	case 2:
		return os.Stderr
	}
	return nil
}

// __c2go_std_isdefault reports whether the std stream is still the startup
// object (the identity gate for freopen/dup2 kernel-level redirects, C side).
//
//go:linkname __c2go_std_isdefault
func __c2go_std_isdefault(which int32) int32 {
	if f := stdLive(which); f != nil && f == stdInit[which] {
		return 1
	}
	return 0
}

// stdErrno is errnoOfPath's portable twin (that one lives in unix-only
// dirent.go): unwrap any nested syscall.Errno, EIO fallback.
func stdErrno(err error) int64 {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		return int64(errno)
	}
	return int64(syscall.EIO)
}

// stdRWMax is io.go's rwMax (unix-only file), portable copy: caps a transfer
// so unsafe.Slice stays in range for an absurd size_t.
const stdRWMax = 0x7ffff000

// stdWriteAll writes b to f in stdRWMax-clamped chunks against the ONE
// snapshot f, mirroring the raw-write shim contract: bytes written, plus the
// errno that stopped a short write (0 on full success). A Write returning
// (0, nil) for nonzero input would loop forever — treat it as EIO.
func stdWriteAll(f *os.File, b []byte) (int, int32) {
	done := 0
	for done < len(b) {
		end := len(b)
		if end-done > stdRWMax {
			end = done + stdRWMax
		}
		m, err := f.Write(b[done:end])
		done += m
		if err != nil {
			return done, int32(stdErrno(err))
		}
		if m == 0 {
			return done, int32(syscall.EIO)
		}
	}
	return done, 0
}

//go:linkname __c2go_std_write
func __c2go_std_write(which int32, buf unsafe.Pointer, n uint64) int64 {
	if buf == nil && n > 0 {
		return -int64(syscall.EFAULT) // #658 M12: kernel-fd parity (EFAULT, not a Go panic)
	}
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	m, err := f.Write(unsafe.Slice((*byte)(buf), n))
	if m > 0 {
		return int64(m) // partial-with-error: report progress; the caller re-errors
	}
	if err != nil {
		return -stdErrno(err)
	}
	return 0
}

// __c2go_std_writev is the std FILE write callback's single-crossing form
// (source/stdio.c __std_go_write): one call carries BOTH musl segments — the
// buffered [wbase,wpos) bytes and the caller's buffer — so the live os.Std*
// is snapshotted ONCE per logical flush and a single fwrite cannot split
// across two sinks under a (contract-violating) concurrent swap. The segments
// are written separately, preserving write boundaries (pipe atomicity) —
// never concatenated. Returns the CALLER-segment bytes written; *eout gets
// the errno that stopped the flush (0 = full success), matching musl
// __stdio_write's error contract in the C wrapper.
//
//go:linkname __c2go_std_writev
func __c2go_std_writev(which int32, b0 unsafe.Pointer, l0 uint64, b1 unsafe.Pointer, l1 uint64, eout *int32) uint64 {
	f := stdLive(which) // ONE snapshot for the whole flush
	if f == nil {
		*eout = int32(syscall.EBADF)
		return 0
	}
	if (b0 == nil && l0 > 0) || (b1 == nil && l1 > 0) {
		*eout = int32(syscall.EFAULT) // #658 M12
		return 0
	}
	if l0 > 0 {
		if _, e := stdWriteSeg(f, b0, l0); e != 0 {
			*eout = e
			return 0 // buffered segment failed: nothing of the caller's is out
		}
	}
	var k int
	if l1 > 0 {
		var e int32
		k, e = stdWriteSeg(f, b1, l1)
		if e != 0 {
			*eout = e
			return uint64(k) // partial caller-segment count
		}
	}
	*eout = 0
	return uint64(k)
}

//go:linkname __c2go_std_read
func __c2go_std_read(which int32, buf unsafe.Pointer, n uint64) int64 {
	if buf == nil && n > 0 {
		return -int64(syscall.EFAULT) // #658 M12
	}
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	m, err := f.Read(unsafe.Slice((*byte)(buf), n))
	if m > 0 {
		return int64(m)
	}
	if err == nil || errors.Is(err, io.EOF) {
		return 0 // EOF
	}
	return -stdErrno(err)
}

//go:linkname __c2go_std_seek
func __c2go_std_seek(which int32, off int64, whence int32) int64 {
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	r, err := f.Seek(off, int(whence))
	if err != nil {
		return -stdErrno(err)
	}
	return r
}

// ── Portable std-descriptor helpers (os.File-native; shared by the unix
// syscall shims and the Windows CRT wrappers) ─────────────────────────────

// stdWriteSeg writes the n bytes at p through stdWriteAll in stdRWMax windows,
// so a C size_t never becomes one unbounded unsafe.Slice (#658 M12 — the
// sibling __c2go_std_write clamps the same way).
func stdWriteSeg(f *os.File, p unsafe.Pointer, n uint64) (int, int32) {
	total := 0
	for off := uint64(0); off < n; {
		k := n - off
		if k > stdRWMax {
			k = stdRWMax
		}
		m, e := stdWriteAll(f, unsafe.Slice((*byte)(unsafe.Add(p, off)), k))
		total += m
		if e != 0 {
			return total, e
		}
		off += k
	}
	return total, 0
}

// stdPread / stdPwrite: positional I/O on the live object (os.File native).
func stdPread(which int32, b []byte, off int64) int64 {
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	m, err := f.ReadAt(b, off)
	if m > 0 {
		return int64(m)
	}
	if err == nil || errors.Is(err, io.EOF) {
		return 0 // at/past EOF: POSIX pread returns 0, not an error (#655 H2)
	}
	return -stdErrno(err)
}

func stdPwrite(which int32, b []byte, off int64) int64 {
	f := stdLive(which)
	if f == nil {
		return -int64(syscall.EBADF)
	}
	m, err := f.WriteAt(b, off)
	if m > 0 {
		return int64(m)
	}
	if err != nil {
		return -stdErrno(err)
	}
	return 0
}

// stdSync / stdTruncate: fsync/fdatasync/ftruncate on the live object.
func stdSync(which int32) int32 {
	f := stdLive(which)
	if f == nil {
		return -int32(syscall.EBADF)
	}
	if err := f.Sync(); err != nil {
		return -int32(stdErrno(err))
	}
	return 0
}

func stdTruncate(which int32, length int64) int32 {
	f := stdLive(which)
	if f == nil {
		return -int32(syscall.EBADF)
	}
	if err := f.Truncate(length); err != nil {
		return -int32(stdErrno(err))
	}
	return 0
}

// isStdFd is the fd-layer routing predicate.
func isStdFd(fd int32) bool { return fd >= 0 && fd <= 2 }

// C-callable exports of the portable std helpers (the Windows CRT wrappers in
// source/fsops_windows.c call these directly; the unix shims call the plain
// helpers in-package).

//go:linkname __c2go_std_pread
func __c2go_std_pread(which int32, buf unsafe.Pointer, n uint64, off int64) int64 {
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	return stdPread(which, unsafe.Slice((*byte)(buf), n), off)
}

//go:linkname __c2go_std_pwrite
func __c2go_std_pwrite(which int32, buf unsafe.Pointer, n uint64, off int64) int64 {
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	return stdPwrite(which, unsafe.Slice((*byte)(buf), n), off)
}

//go:linkname __c2go_std_sync
func __c2go_std_sync(which int32) int32 { return stdSync(which) }

//go:linkname __c2go_std_truncate
func __c2go_std_truncate(which int32, length int64) int32 { return stdTruncate(which, length) }
