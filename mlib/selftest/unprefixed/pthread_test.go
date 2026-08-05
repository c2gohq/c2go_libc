// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestPthreadHeader(t *testing.T) {
	if got := MlibPthreadUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed pthread selftest = %d", got)
	}
}
