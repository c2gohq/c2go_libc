//go:build unix

package selftest

import "testing"

// TestNftw drives the in-C <ftw.h> exercise (selftest/source/nftw_selftest.c,
// #675 C wave 2b): tree build, pre-order and FTW_DEPTH walks with type/level
// accounting, early-stop propagation, fd_limit<=0 no-op, ENOENT, and a
// dogfooded FTW_DEPTH removal. The fn callback is C-invoked, so the whole
// exercise is a C driver (qsort_selftest precedent).
func TestNftw(t *testing.T) {
	if code := NftwSelftest(); code != 0 {
		t.Fatalf("nftw_selftest failed with code %d", code)
	}
}
