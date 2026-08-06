// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedStandardStreamRetirement(t *testing.T) {
	if got := MlibStdioRetireUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed standard-stream retirement selftest = %d", got)
	}
}
