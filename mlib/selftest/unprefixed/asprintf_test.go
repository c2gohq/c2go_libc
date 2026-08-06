// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedAsprintf(t *testing.T) {
	if got := MlibAsprintfUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed asprintf selftest = %d", got)
	}
}
