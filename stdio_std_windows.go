// stdio_std_windows.go — Windows freopen(std stream) rebind (task #644).
//
// On unix, freopen(stdout, path) is a kernel-level dup3 onto the startup fd,
// which os.Stdout (wrapping that fd) follows automatically. Windows has no
// such lever: os.Std* is HANDLE-backed, and a CRT _dup2 onto CRT fd 0/1/2
// moves only the CRT's private table. So the Windows redirect uses the OTHER
// sanctioned mechanism of the virtualized-std design: REPLACE the os.Std*
// VARIABLE (exactly a Go-side redirect, which every std operation re-reads
// live). freopen (source/stdio.c) hands us the new file's underlying HANDLE;
// we DUPLICATE it (so the CRT fd keeps sole ownership of the original and the
// C side can fclose its temporary stream normally — no leaked CRT slot) and
// wrap the duplicate in a fresh os.File assigned to os.Std*.
//
// Nothing is mutated under a host-owned object (unlike the unix dup3, which
// is identity-gated): a prior host swap just means its object stops being the
// live sink — the semantics of "C redirected stdout". A rebind file that is
// later replaced again becomes unreachable and its handle is reclaimed by
// os.File's cleanup.

//go:build windows

package libc

import (
	"os"
	"syscall"

	_ "unsafe" // for //go:linkname
)

var stdRebindName = [3]string{"/dev/stdin", "/dev/stdout", "/dev/stderr"}

//go:linkname __c2go_std_rebind
func __c2go_std_rebind(which int32, h int64) int32 {
	if which < 0 || which > 2 {
		return -errEINVAL
	}
	proc, err := syscall.GetCurrentProcess() // pseudo-handle; no close needed
	if err != nil {
		return -winErrno(err)
	}
	var dup syscall.Handle
	// Duplicate with the source's access rights; inheritable, matching the
	// startup std handles (os/exec duplicates for children either way).
	if err := syscall.DuplicateHandle(proc, syscall.Handle(h), proc, &dup,
		0, true, syscall.DUPLICATE_SAME_ACCESS); err != nil {
		return -winErrno(err)
	}
	nf := os.NewFile(uintptr(dup), stdRebindName[which])
	if nf == nil {
		syscall.CloseHandle(dup)
		return -errEBADF
	}
	switch which {
	case 0:
		os.Stdin = nf
	case 1:
		os.Stdout = nf
	case 2:
		os.Stderr = nf
	}
	return 0
}
