// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestDirentHeader(t *testing.T) {
	if got := MlibDirentPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced dirent selftest = %d", got)
	}
}
