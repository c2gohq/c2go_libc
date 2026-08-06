// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedDirentRetirement(t *testing.T) {
	if got := MlibDirentRetireUnprefixedSelftest(); got != 0 {
		t.Fatalf("managed DIR/scandir retirement selftest = %d", got)
	}
}
