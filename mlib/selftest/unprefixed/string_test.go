// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedStringUnprefixed(t *testing.T) {
	if got := MlibStringUnprefixedSelftest(); got != 0 {
		t.Fatalf("managed string unprefixed selftest = %d", got)
	}
}
