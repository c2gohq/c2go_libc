// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import "runtime"

// ForceGC is called from the generated C fixture while managed C roots are
// live, validating their stack/global/heap pointer metadata.
func ForceGC() {
	runtime.GC()
}
