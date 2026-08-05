// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import (
	"testing"
)

func TestSemaphoreHeader(t *testing.T) {
	if got := MlibSemUnprefixedSelftest(); got != 0 {
		t.Fatalf("C unprefixed semaphore selftest = %d", got)
	}
}
