// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedSyncRetirement(t *testing.T) {
	if got := MlibSyncRetireUnprefixedSelftest(); got != 0 {
		t.Fatalf("managed synchronization retirement selftest = %d", got)
	}
}
