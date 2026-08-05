// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedGlobHeader(t *testing.T) {
	if got := MlibGlobPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced glob selftest = %d", got)
	}
}
