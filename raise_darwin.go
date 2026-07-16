// raise_darwin.go — raise()'s self-signal, macOS side (source/signal.c).
//
//go:build darwin

package libc

import (
	"syscall"

	_ "unsafe" // for //go:linkname
)

// __c2go_raise self-delivers sig via kill(getpid). Darwin's Go syscall layer has
// no tgkill/gettid, so delivery is process-directed rather than thread-directed;
// for raise's uses (abort, programmatic termination / notification) that is
// equivalent. Returns 0, or -errno on failure.
//go:linkname __c2go_raise
func __c2go_raise(sig int32) int32 {
	if err := syscall.Kill(syscall.Getpid(), syscall.Signal(sig)); err != nil {
		return -errnoOf(err)
	}
	return 0
}
