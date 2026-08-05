// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import (
	"testing"
)

func TestSemaphoreHeader(t *testing.T) {
	if got := MlibSemPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced semaphore selftest = %d", got)
	}
}
