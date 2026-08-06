// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedStandardStreamRetirement(t *testing.T) {
	if got := MlibStdioRetirePrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced standard-stream retirement selftest = %d", got)
	}
}
