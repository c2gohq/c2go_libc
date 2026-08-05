//go:build unix

// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "testing"

func TestManagedFtwHeader(t *testing.T) {
	if got := MlibFtwPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced ftw selftest = %d", got)
	}
}
