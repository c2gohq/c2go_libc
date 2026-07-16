//go:build unix

package selftest

import "testing"

// TestScandir drives the in-C exercise of scandir + alphasort/versionsort
// (selftest/source/scandir_selftest.c). scandir's sel/cmp are C callbacks
// called from C (cmp via qsort), so they run from a C driver rather than a Go
// test. It covers alphasort ordering, versionsort's numeric ordering (a9 before
// a10), a C sel filter, and the ENOENT error path.
func TestScandir(t *testing.T) {
	if code := ScandirSelftest(); code != 0 {
		t.Fatalf("scandir_selftest failed with code %d", code)
	}
}
