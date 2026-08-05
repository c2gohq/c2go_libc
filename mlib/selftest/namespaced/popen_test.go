// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedPopen(t *testing.T) {
	if got := MlibPopenPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced popen selftest = %d", got)
	}
}
