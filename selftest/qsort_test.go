// Package selftest holds in-C exercises of c2go-libc functions whose comparator
// or callback is invoked FROM C (qsort/bsearch), so they cannot be driven by a
// Go closure directly. Keeping them in a separate package keeps the C driver
// (QsortSelftest) off the public c2go-libc API surface.
package selftest

import (
	"testing"

	// Force-link c2go-libc so its C-implemented qsort/bsearch symbols
	// (referenced cross-package by QsortSelftest's Plan 9 .s) are present.
	_ "github.com/c2gohq/c2go_libc"
)

// TestQsort drives the in-C qsort/bsearch exercise (selftest/source/
// qsort_selftest.c): a C comparator sorts and searches across several sizes and
// input patterns, returning 0 on success.
func TestQsort(t *testing.T) {
	if code := QsortSelftest(); code != 0 {
		t.Fatalf("QsortSelftest failed with code %d", code)
	}
}
