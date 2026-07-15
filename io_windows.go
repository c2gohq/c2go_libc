// io_windows.go — the Go side of the Windows path operations (unlink/rmdir/
// rename), the symmetric counterpart of the Unix syscall shims in io.go.
//
// The Windows fd PRIMITIVES (open/read/write/close/lseek/dup2) stay in
// source/io_windows.c over msvcrt: Go's Windows syscall layer is pure Win32
// (HANDLE-based) with no small-int fd table, so it cannot back read(2)/open(2).
// The PATH operations are different — they take a path, not an fd, so Go CAN
// perform them without the CRT fd table. Per c2go-libc's Go-first rule (prefer
// the Go runtime over a host-DLL import when Go can do the job), they live here
// rather than over msvcrt's _unlink/_rmdir/rename. This also FIXES a real
// fidelity bug: msvcrt's rename fails when the destination already exists,
// whereas Go's MoveFileEx(MOVEFILE_REPLACE_EXISTING) gives the POSIX atomic-
// replace semantics ported C expects.
//
// Shim contract mirrors io.go: return 0 on success or -errno (MinGW value) on
// failure; source/io_windows.c applies musl's __syscall_ret shape
// (`if (r < 0) { errno = -r; return -1; }`).
//
//go:build windows

package libc

import (
	"errors"
	"os"
	"path/filepath"
	"syscall"
	"unsafe"
)

// winErrno reproduces msvcrt's _dosmaperr: the underlying error of a failed
// DeleteFile/RemoveDirectory/os.Rename is a syscall.Errno whose value is the raw
// Windows system error code, which this maps to the MinGW-native errno a ported C
// caller expects — so the Go-backed path ops report the same errno the msvcrt fd
// layer (source/io_windows.c) would for the identical failure. errors.As unwraps
// the *os.LinkError/*os.PathError that os.Rename wraps its Errno in. A non-Errno
// error, or a code absent from the table, yields EINVAL (the CRT's default),
// except the exec-format range 188..202 which _dosmaperr maps to ENOEXEC.
func winErrno(err error) int32 {
	var e syscall.Errno
	if !errors.As(err, &e) {
		return errEINVAL
	}
	switch e {
	case 2, 3, 15, 18, 53, 67, 161, 206:
		// FILE/PATH_NOT_FOUND, INVALID_DRIVE, NO_MORE_FILES, BAD_NETPATH,
		// BAD_NET_NAME, BAD_PATHNAME, FILENAME_EXCED_RANGE
		return errENOENT
	case 4:
		return errEMFILE // TOO_MANY_OPEN_FILES
	case 5, 16, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 36,
		65, 82, 83, 108, 132, 158, 167:
		// ACCESS_DENIED, CURRENT_DIRECTORY, the WRITE_PROTECT..SHARING_BUFFER_EXCEEDED
		// range, NETWORK_ACCESS_DENIED, CANNOT_MAKE, FAIL_I24, DRIVE_LOCKED,
		// SEEK_ON_DEVICE, NOT_LOCKED, LOCK_FAILED
		return errEACCES
	case 6, 114, 130:
		return errEBADF // INVALID_HANDLE, INVALID_TARGET_HANDLE, DIRECT_ACCESS_HANDLE
	case 7, 8, 9, 1816:
		return errENOMEM // ARENA_TRASHED, NOT_ENOUGH_MEMORY, INVALID_BLOCK, NOT_ENOUGH_QUOTA
	case 10:
		return errE2BIG // BAD_ENVIRONMENT
	case 11:
		return errENOEXEC // BAD_FORMAT
	case 17:
		return errEXDEV // NOT_SAME_DEVICE
	case 39, 112:
		return errENOSPC // HANDLE_DISK_FULL, DISK_FULL
	case 80, 183:
		return errEEXIST // FILE_EXISTS, ALREADY_EXISTS
	case 89, 164, 215:
		return errEAGAIN // NO_PROC_SLOTS, MAX_THRDS_REACHED, NESTING_NOT_ALLOWED
	case 109:
		return errEPIPE // BROKEN_PIPE
	case 128, 129:
		return errECHILD // WAIT_NO_CHILDREN, CHILD_NOT_COMPLETE
	case 145:
		return errENOTEMPTY // DIR_NOT_EMPTY
	}
	if e >= 188 && e <= 202 {
		return errENOEXEC // INVALID_STARTING_CODESEG..INFLOOP_IN_RELOC_CHAIN
	}
	return errEINVAL
}

//go:linkname __c2go_syscall_unlink
func __c2go_syscall_unlink(path *byte) int32 {
	p, err := syscall.UTF16PtrFromString(cstr(path))
	if err != nil {
		return -errEINVAL
	}
	if err := syscall.DeleteFile(p); err != nil {
		return -winErrno(err)
	}
	return 0
}

//go:linkname __c2go_syscall_rmdir
func __c2go_syscall_rmdir(path *byte) int32 {
	p, err := syscall.UTF16PtrFromString(cstr(path))
	if err != nil {
		return -errEINVAL
	}
	if err := syscall.RemoveDirectory(p); err != nil {
		return -winErrno(err)
	}
	return 0
}

//go:linkname __c2go_syscall_rename
func __c2go_syscall_rename(oldp, newp *byte) int32 {
	// os.Rename is MoveFileEx(MOVEFILE_REPLACE_EXISTING) on Windows — the POSIX
	// atomic-replace semantics; the standard syscall package exposes no MoveFileEx.
	if err := os.Rename(cstr(oldp), cstr(newp)); err != nil {
		return -winErrno(err)
	}
	return 0
}

// ── #647: the remaining PATH family, Go-first like unlink/rmdir/rename ──────

//go:linkname __c2go_syscall_truncate
func __c2go_syscall_truncate(path *byte, length int64) int32 {
	if err := os.Truncate(cstr(path), length); err != nil {
		return -winErrno(err)
	}
	return 0
}

//go:linkname __c2go_syscall_link
func __c2go_syscall_link(from, to *byte) int32 {
	if err := os.Link(cstr(from), cstr(to)); err != nil {
		return -winErrno(err)
	}
	return 0
}

// symlink: os.Symlink (CreateSymbolicLink). Windows requires a privilege or
// Developer Mode for it; an unprivileged failure surfaces as EACCES — honest,
// not masked.
//
//go:linkname __c2go_syscall_symlink
func __c2go_syscall_symlink(target, linkpath *byte) int32 {
	if err := os.Symlink(cstr(target), cstr(linkpath)); err != nil {
		return -winErrno(err)
	}
	return 0
}

// readlink fills buf (NOT NUL-terminated) and returns the byte count,
// truncating to bufsiz — the POSIX contract, mirroring fsops.go.
//
//go:linkname __c2go_syscall_readlink
func __c2go_syscall_readlink(path *byte, buf *byte, bufsiz uint64) int64 {
	target, err := os.Readlink(cstr(path))
	if err != nil {
		return -int64(winErrno(err))
	}
	b := []byte(target)
	if uint64(len(b)) > bufsiz {
		b = b[:bufsiz]
	}
	if len(b) > 0 {
		copy(unsafe.Slice(buf, len(b)), b)
	}
	return int64(len(b))
}

//go:linkname __c2go_syscall_chdir
func __c2go_syscall_chdir(path *byte) int32 {
	if err := os.Chdir(cstr(path)); err != nil {
		return -winErrno(err)
	}
	return 0
}

// getcwd fills buf NUL-terminated; ERANGE when it does not fit (POSIX),
// mirroring fsops.go's unix shim.
//
//go:linkname __c2go_syscall_getcwd
func __c2go_syscall_getcwd(buf *byte, size uint64) int32 {
	wd, err := os.Getwd()
	if err != nil {
		return -winErrno(err)
	}
	if uint64(len(wd))+1 > size {
		return -errERANGE
	}
	dst := unsafe.Slice(buf, len(wd)+1)
	copy(dst, wd)
	dst[len(wd)] = 0
	return 0
}

// __c2go_realpath canonicalises via EvalSymlinks + Abs (same bridge contract
// as the unix fsops.go one: write at most `size` bytes NUL-terminated, or
// -errno).
//
//go:linkname __c2go_realpath
func __c2go_realpath(path *byte, buf *byte, size uint64) int32 {
	p, err := filepath.EvalSymlinks(cstr(path))
	if err != nil {
		return -winErrno(err)
	}
	abs, err := filepath.Abs(p)
	if err != nil {
		return -winErrno(err)
	}
	if uint64(len(abs))+1 > size {
		return -errENAMETOOLONG
	}
	dst := unsafe.Slice(buf, len(abs)+1)
	copy(dst, abs)
	dst[len(abs)] = 0
	return 0
}

// ── #647: fd-family bridges that need Win32 rather than the CRT ────────────

// __c2go_pread_h / __c2go_pwrite_h: positional I/O on an open HANDLE
// (recovered by the C wrapper via _get_osfhandle) using ReadFile/WriteFile
// with an OVERLAPPED offset — Win32's native pread/pwrite. Caveat (documented
// in fsops_windows.c): on a synchronous handle the file pointer DOES move,
// unlike POSIX pread; MinGW-w64 offers no better primitive.
//
//go:linkname __c2go_closehandle
func __c2go_closehandle(h int64) {
	syscall.CloseHandle(syscall.Handle(h)) // #658 M8: error-path cleanup only
}

//go:linkname __c2go_pread_h
func __c2go_pread_h(h int64, buf unsafe.Pointer, n uint64, off int64) int64 {
	if n == 0 {
		return 0
	}
	if buf == nil {
		return -int64(errEFAULT) // #658 M12 parity
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	// #658 M7: on a synchronous handle the OVERLAPPED read moves the file
	// pointer; save/restore it so pread doesn't disturb a subsequent read().
	// Racy only under concurrent same-fd I/O — which moves the pointer anyway.
	cur, cerr := syscall.Seek(syscall.Handle(h), 0, 1)
	var done uint32
	ov := syscall.Overlapped{Offset: uint32(off), OffsetHigh: uint32(off >> 32)}
	err := syscall.ReadFile(syscall.Handle(h), unsafe.Slice((*byte)(buf), n), &done, &ov)
	if cerr == nil {
		syscall.Seek(syscall.Handle(h), cur, 0)
	}
	if err != nil {
		if err == syscall.ERROR_HANDLE_EOF {
			return 0
		}
		return -int64(winErrno(err))
	}
	return int64(done)
}

//go:linkname __c2go_pwrite_h
func __c2go_pwrite_h(h int64, buf unsafe.Pointer, n uint64, off int64) int64 {
	if n == 0 {
		return 0
	}
	if buf == nil {
		return -int64(errEFAULT) // #658 M12 parity
	}
	if n > stdRWMax {
		n = stdRWMax
	}
	// #658 M7: save/restore the file pointer (see __c2go_pread_h).
	cur, cerr := syscall.Seek(syscall.Handle(h), 0, 1)
	var done uint32
	ov := syscall.Overlapped{Offset: uint32(off), OffsetHigh: uint32(off >> 32)}
	err := syscall.WriteFile(syscall.Handle(h), unsafe.Slice((*byte)(buf), n), &done, &ov)
	if cerr == nil {
		syscall.Seek(syscall.Handle(h), cur, 0)
	}
	if err != nil {
		return -int64(winErrno(err))
	}
	return int64(done)
}

// __c2go_std_dup_handle: dup(std) on Windows — snapshot the live os.Std*'s
// HANDLE via DuplicateHandle (the #644 rebind mechanism); the C wrapper wraps
// the duplicate into a CRT fd with _open_osfhandle. Returns the handle value
// or -errno.
//
//go:linkname __c2go_std_dup_handle
func __c2go_std_dup_handle(which int32) int64 {
	f := stdLive(which)
	if f == nil {
		return -int64(errEBADF)
	}
	proc, err := syscall.GetCurrentProcess()
	if err != nil {
		return -int64(winErrno(err))
	}
	var dup syscall.Handle
	// Reach the handle via SyscallConn (side-effect-free), then duplicate it.
	sc, err := f.SyscallConn()
	if err != nil {
		return -int64(winErrno(err))
	}
	var r int64
	cerr := sc.Control(func(h uintptr) {
		if err := syscall.DuplicateHandle(proc, syscall.Handle(h), proc, &dup,
			0, true, syscall.DUPLICATE_SAME_ACCESS); err != nil {
			r = -int64(winErrno(err))
			return
		}
		r = int64(dup)
	})
	if cerr != nil {
		return -int64(winErrno(cerr))
	}
	return r
}

// stdIsatty (windows): identity-gated console probe — a host-swapped stream
// is a pipe/file, report 0 without touching the host object; on the startup
// object probe GetConsoleMode under SyscallConn (no Fd() side effects).
func stdIsatty(which int32) int32 {
	if __c2go_std_isdefault(which) == 0 {
		return 0
	}
	f := stdLive(which)
	if f == nil {
		return 0
	}
	sc, err := f.SyscallConn()
	if err != nil {
		return 0
	}
	var r int32
	_ = sc.Control(func(h uintptr) {
		var mode uint32
		if syscall.GetConsoleMode(syscall.Handle(h), &mode) == nil {
			r = 1
		}
	})
	return r
}

// __c2go_std_isatty_win is stdIsatty's C-callable form (source/fsops_windows.c).
//
//go:linkname __c2go_std_isatty_win
func __c2go_std_isatty_win(which int32) int32 { return stdIsatty(which) }

// __c2go_ftruncate_h: 64-bit clean ftruncate on an open HANDLE (msvcrt's
// _chsize takes a 32-bit long; ucrt's _chsize_s is absent from msvcrt.dll).
//
//go:linkname __c2go_ftruncate_h
func __c2go_ftruncate_h(h int64, length int64) int32 {
	if err := syscall.Ftruncate(syscall.Handle(h), length); err != nil {
		return -winErrno(err)
	}
	return 0
}
