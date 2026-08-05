// SPDX-License-Identifier: AGPL-3.0-only

package unprefixed

import "runtime"

func ForceGC() {
	runtime.GC()
}
