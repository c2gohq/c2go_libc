// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedStdioHeader(t *testing.T) {
	if got := MlibStdioUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed stdio selftest = %d", got)
	}
}
