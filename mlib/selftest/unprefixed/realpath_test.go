// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedRealpath(t *testing.T) {
	if got := MlibRealpathUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed realpath selftest = %d", got)
	}
}
