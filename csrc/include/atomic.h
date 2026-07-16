/* atomic.h — c2go shim standing in for musl's src/internal/atomic.h, for
 * verbatim-musl TUs built from the fork whose `#include "atomic.h"` falls
 * through to the include path (quote-include finds nothing next to the TU).
 * Only the pure BIT helpers musl code actually uses are provided (qsort's
 * ntz); REAL atomics are never taken from here — concurrency primitives live
 * on the Go side. NOTE: if a future phase puts musl's src/internal on the
 * include path, remove this shim or it will shadow the real header. */
#ifndef _C2GO_ATOMIC_SHIM_H
#define _C2GO_ATOMIC_SHIM_H

static inline int a_ctz_64(unsigned long long x)
{
	return __builtin_ctzll(x);
}

static inline int a_ctz_l(unsigned long x)
{
	return __builtin_ctzl(x);
}

#endif
