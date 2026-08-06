// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedRegexPrefixed(t *testing.T) {
	if got := MlibRegexPrefixedSelftest(); got != 0 {
		t.Fatalf("managed regex prefixed selftest = %d", got)
	}
}
