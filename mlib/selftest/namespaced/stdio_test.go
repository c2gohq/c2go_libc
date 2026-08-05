// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedStdioHeader(t *testing.T) {
	if got := MlibStdioPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced stdio selftest = %d", got)
	}
}
