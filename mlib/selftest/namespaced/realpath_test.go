// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedRealpath(t *testing.T) {
	if got := MlibRealpathPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced realpath selftest = %d", got)
	}
}
