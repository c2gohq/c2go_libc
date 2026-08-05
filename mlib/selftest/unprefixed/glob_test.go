// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedGlobHeader(t *testing.T) {
	if got := MlibGlobUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed glob selftest = %d", got)
	}
}
