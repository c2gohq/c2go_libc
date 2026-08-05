//go:build unix

// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedFtwHeader(t *testing.T) {
	if got := MlibFtwUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed ftw selftest = %d", got)
	}
}
