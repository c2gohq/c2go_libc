// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedRegexUnprefixed(t *testing.T) {
	if got := MlibRegexUnprefixedSelftest(); got != 0 {
		t.Fatalf("managed regex unprefixed selftest = %d", got)
	}
}
