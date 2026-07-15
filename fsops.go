// fsops.go — Go bridges for the remaining POSIX fd / path ops
// (source/fsops_posix.c). Same shim contract as io.go: return the syscall result
// on success or -errno on failure; the C wrapper turns -errno into errno + -1.
// EINTR is retried (spurious under the Go runtime). fdatasync and isatty are
// per-OS (fsops_darwin.go / fsops_linux.go).
//
// Buffer-filling bridges (pread/readlink/getcwd/pipe) write the caller's memory
// only AFTER the blocking syscall; the pointer parameters are tracked (copystack-
// adjusted), so nothing is laundered through uintptr.
//
//go:build unix

package libc

import (
	"errors"
	"os"
	"path/filepath"
	"syscall"
	"unsafe"
)

// __c2go_realpath canonicalises path (resolve symlinks + make absolute, requiring
// the path to exist) into buf (NUL-terminated), or returns -errno. It backs the C
// realpath wrapper. filepath.EvalSymlinks returns a wrapped *os.PathError, so the
// errno is extracted with errors.As (errnoOf only unwraps a bare syscall.Errno).
//
// Order matters (#655 H4): EvalSymlinks FIRST, then Abs — same as the Windows
// version. filepath.Abs lexically Cleans, so Abs-first would fold "sym/.."
// away before the symlink is resolved, breaking POSIX realpath (".." applies
// to the symlink TARGET, not the link name).
//
//go:linkname __c2go_realpath
func __c2go_realpath(path *byte, buf *byte, size uint64) int32 {
	abs, err := filepath.EvalSymlinks(cstr(path))
	if err == nil {
		abs, err = filepath.Abs(abs)
	}
	if err != nil {
		var errno syscall.Errno
		if errors.As(err, &errno) {
			return -int32(errno)
		}
		return -int32(syscall.EIO)
	}
	if size == 0 || uint64(len(abs))+1 > size {
		return -int32(syscall.ENAMETOOLONG)
	}
	dst := unsafe.Slice(buf, size)
	copy(dst, abs)
	dst[len(abs)] = 0
	return 0
}

//go:linkname __c2go_syscall_pread
func __c2go_syscall_pread(fd int32, buf unsafe.Pointer, n uint64, off int64) int64 {
	if isStdFd(fd) {
		if n == 0 {
			return 0
		}
		if n > stdRWMax {
			n = stdRWMax
		}
		return stdPread(fd, unsafe.Slice((*byte)(buf), n), off) // virtualized std fd
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	b := unsafe.Slice((*byte)(buf), n)
	for {
		m, err := syscall.Pread(int(fd), b, off)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(m)
	}
}

//go:linkname __c2go_syscall_pwrite
func __c2go_syscall_pwrite(fd int32, buf unsafe.Pointer, n uint64, off int64) int64 {
	if isStdFd(fd) {
		if n == 0 {
			return 0
		}
		if n > stdRWMax {
			n = stdRWMax
		}
		return stdPwrite(fd, unsafe.Slice((*byte)(buf), n), off) // virtualized std fd
	}
	if n == 0 {
		return 0
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	b := unsafe.Slice((*byte)(buf), n)
	for {
		m, err := syscall.Pwrite(int(fd), b, off)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(m)
	}
}

//go:linkname __c2go_syscall_dup
func __c2go_syscall_dup(fd int32) int32 {
	if isStdFd(fd) {
		return stdDup(fd) // snapshot the live target onto a raw fd (Control access)
	}
	nfd, err := syscall.Dup(int(fd))
	if err != nil {
		return -errnoOf(err)
	}
	return int32(nfd)
}

//go:linkname __c2go_syscall_pipe
func __c2go_syscall_pipe(fds unsafe.Pointer) int32 {
	var p [2]int
	for {
		err := syscall.Pipe(p[:])
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		break
	}
	arr := (*[2]int32)(fds)
	arr[0] = int32(p[0])
	arr[1] = int32(p[1])
	return 0
}

//go:linkname __c2go_syscall_fsync
func __c2go_syscall_fsync(fd int32) int32 {
	if isStdFd(fd) {
		return stdSync(fd)
	}
	for {
		err := syscall.Fsync(int(fd))
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_ftruncate
func __c2go_syscall_ftruncate(fd int32, length int64) int32 {
	if isStdFd(fd) {
		return stdTruncate(fd, length)
	}
	for {
		err := syscall.Ftruncate(int(fd), length)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_truncate
func __c2go_syscall_truncate(path *byte, length int64) int32 {
	name := cstr(path)
	for {
		err := syscall.Truncate(name, length)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_link
func __c2go_syscall_link(from, to *byte) int32 {
	f, t := cstr(from), cstr(to)
	for {
		err := syscall.Link(f, t)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_symlink
func __c2go_syscall_symlink(target, linkpath *byte) int32 {
	tg, lp := cstr(target), cstr(linkpath)
	for {
		err := syscall.Symlink(tg, lp)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -errnoOf(err)
		}
		return 0
	}
}

//go:linkname __c2go_syscall_readlink
func __c2go_syscall_readlink(path *byte, buf unsafe.Pointer, bufsiz uint64) int64 {
	name := cstr(path)
	if bufsiz == 0 {
		return 0
	}
	if bufsiz > stdRWMax {
		bufsiz = stdRWMax
	}
	b := unsafe.Slice((*byte)(buf), bufsiz)
	for {
		n, err := syscall.Readlink(name, b)
		if err == syscall.EINTR {
			continue
		}
		if err != nil {
			return -int64(errnoOf(err))
		}
		return int64(n)
	}
}

//go:linkname __c2go_syscall_chdir
func __c2go_syscall_chdir(path *byte) int32 {
	// os.Chdir (not raw syscall.Chdir) so Go's own view of the cwd stays consistent
	// with the C world: the process cwd is shared by every goroutine, and os.Getwd
	// caches it — a raw syscall.Chdir would silently desync that cache and every
	// os-relative path Go resolves afterwards. os.Chdir wraps errno in *PathError,
	// so unwrap with errors.As (errnoOf only reads a bare syscall.Errno).
	if err := os.Chdir(cstr(path)); err != nil {
		var errno syscall.Errno
		if errors.As(err, &errno) {
			return -int32(errno)
		}
		return -int32(syscall.EIO)
	}
	return 0
}

// getcwd fills buf with the NUL-terminated cwd; ERANGE if it does not fit.
//go:linkname __c2go_syscall_getcwd
func __c2go_syscall_getcwd(buf unsafe.Pointer, size uint64) int32 {
	wd, err := os.Getwd() // os.Getwd (not syscall.Getwd) uses Go's own cwd cache
	if err != nil {
		var errno syscall.Errno
		if errors.As(err, &errno) {
			return -int32(errno)
		}
		return -int32(syscall.EIO)
	}
	if uint64(len(wd))+1 > size {
		return -int32(syscall.ERANGE)
	}
	b := unsafe.Slice((*byte)(buf), size)
	copy(b, wd)
	b[len(wd)] = 0
	return 0
}
