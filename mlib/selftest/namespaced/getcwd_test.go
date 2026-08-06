// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedGetcwd(t *testing.T) {
	if got := MlibGetcwdPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced getcwd selftest = %d", got)
	}
}
