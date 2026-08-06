// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "testing"

func TestManagedGetcwd(t *testing.T) {
	if got := MlibGetcwdUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed getcwd selftest = %d", got)
	}
}
