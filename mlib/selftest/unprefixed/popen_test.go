// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedPopen(t *testing.T) {
	if got := MlibPopenUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed popen selftest = %d", got)
	}
}
