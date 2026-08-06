// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedSyncRetirement(t *testing.T) {
	if got := MlibSyncRetirePrefixedSelftest(); got != 0 {
		t.Fatalf("managed synchronization retirement selftest = %d", got)
	}
}
