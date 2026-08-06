// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedStringPrefixed(t *testing.T) {
	if got := MlibStringPrefixedSelftest(); got != 0 {
		t.Fatalf("managed string prefixed selftest = %d", got)
	}
}
