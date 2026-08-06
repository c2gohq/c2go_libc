// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedAsprintf(t *testing.T) {
	if got := MlibAsprintfPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced asprintf selftest = %d", got)
	}
}
