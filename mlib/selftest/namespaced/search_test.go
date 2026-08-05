// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedSearchHeader(t *testing.T) {
	if got := MlibSearchPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced search selftest = %d", got)
	}
}
