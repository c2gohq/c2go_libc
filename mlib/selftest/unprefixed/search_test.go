// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedSearchHeader(t *testing.T) {
	if got := MlibSearchUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed search selftest = %d", got)
	}
}
