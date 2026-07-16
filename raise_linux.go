// raise_linux.go — raise()'s self-signal, Linux side (source/signal.c).
//
//go:build linux

package libc

import (
	"syscall"

	_ "unsafe" // for //go:linkname
)

// __c2go_raise self-delivers sig to the calling thread via tgkill — musl's raise
// (src/signal/raise.c). It does NOT block application signals around the delivery
// as musl does: c2go has no __block_app_sigs machinery, and the Go runtime, not
// libc, owns the process signal mask. Returns 0, or -errno on failure.
//go:linkname __c2go_raise
func __c2go_raise(sig int32) int32 {
	if err := syscall.Tgkill(syscall.Getpid(), syscall.Gettid(), syscall.Signal(sig)); err != nil {
		return -errnoOf(err)
	}
	return 0
}
