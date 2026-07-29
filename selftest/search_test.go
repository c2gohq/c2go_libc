//go:build unix

package selftest

import "testing"

// TestSearch drives the in-C <search.h> exercise (selftest/source/
// search_selftest.c, #668): tsearch/tfind/tdelete/twalk/tdestroy, lsearch/
// lfind, hsearch(+_r), insque/remque — comparators and the ENTRY-by-value
// hsearch surface are C-only (#671), so the whole family is exercised from C.
func TestSearch(t *testing.T) {
	if code := SearchSelftest(); code != 0 {
		t.Fatalf("search_selftest failed with code %d", code)
	}
}
