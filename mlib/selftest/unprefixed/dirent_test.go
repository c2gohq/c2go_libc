// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestDirentHeader(t *testing.T) {
	if got := MlibDirentUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed dirent selftest = %d", got)
	}
}
