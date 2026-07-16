//go:build unix

package selftest

import "testing"

// TestTermios drives the in-C <termios.h>/<sys/ioctl.h> exercise (selftest/
// source/termios_selftest.c, #675 stage D): FIONREAD on a pipe (positive
// whitelisted ioctl), kernel ENOTTY negatives on non-ttys, EINVAL argument
// validation, whitelist rejection, and the pure cf* bit surface. ioctl is
// variadic (not Go-callable) and cf* take struct termios*, so the whole
// family is exercised from C (search_selftest precedent). Positive tty
// behavior is probe territory (host pty vs the native oracle).
func TestTermios(t *testing.T) {
	if code := TermiosSelftest(); code != 0 {
		t.Fatalf("termios_selftest failed with code %d", code)
	}
}
