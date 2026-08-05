// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestPthreadHeader(t *testing.T) {
	if got := MlibPthreadPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced pthread selftest = %d", got)
	}
}
