// stdio.go — the global lock behind c2go-libc's stdio streams (source/stdio.c).
//
// musl serialises FILE access with a per-thread recursive flockfile. c2go-libc
// uses a single process-wide Go sync.Mutex instead, bridged to C through
// _c2go_file_lock/_c2go_file_unlock — the FLOCK/FUNLOCK macros in stdio.c call
// __lockfile/__unlockfile, which call these.
//
// The mutex is NOT recursive, so the C side never nests a lock on the same
// stream (see the puts() note in stdio.c).
package libc

import (
	"github.com/timandy/routine"

	"sync/atomic"
	"sync"
	_ "unsafe"
)

// _c2go_FILE is the opaque Go handle for the C FILE object (source/stdio.c's
// struct _c2go_FILE — 232 bytes of plain unmanaged memory, emitted NOPTR).
// c2go-bind names FILE* parameters *_c2go_FILE in the generated bindings but
// does NOT emit the type itself, so the package supplies it here. Go code only
// ever holds a *_c2go_FILE as an opaque pointer (handing it to Fwrite/Fflush/…);
// it never reads the fields, so a sized noscan byte blob is sufficient and keeps
// the pointer type noscan.
type _c2go_FILE struct {
	_ [232]byte
}

// FILE is the exported public alias for the opaque handle. A downstream c2go
// package that only forward-declares FILE (via <stdio.h>'s
// `typedef struct _c2go_FILE FILE;`) cannot name the unexported `_c2go_FILE`,
// so c2go-bind qualifies `*_c2go_FILE` → `*libc.FILE` in its generated
// bindings (see libcTypeRefs). The alias makes those pointers type-identical
// to what Fopen/Fwrite/Fclose/… return and accept here.
type FILE = _c2go_FILE

// fileLockTab roots one mutex per open FILE (#659, decision ②): the
// generation-stamped id lives in the FILE's lockid field, created lazily on
// first FLOCK and dropped by fclose. Per-FILE means a blocking fgets(stdin)
// no longer freezes printf(stdout) — the process-global stdioMu it replaces
// did exactly that (the review's biggest undeclared musl deviation).
// fileLock is RECURSIVE by goid (#664, musl parity: musl __lockfile is
// tid-recursive) so flockfile(f) + stdio calls + funlockfile(f) compose, and
// nested internal FLOCKs are tolerated. owner/count are only touched by the
// holder (owner cmp is the sole cross-goroutine read — atomic).
type fileLock struct {
	mu    sync.Mutex
	owner atomic.Uint64 // goid of the holder, 0 = unowned
	count int32         // recursion depth, guarded by ownership
}

var fileLockTab handleTable[fileLock]

//go:linkname _c2go_file_lock
func _c2go_file_lock(idp *uint64) {
	st := fileLockTab.lazyInit(idp, func() *fileLock { return new(fileLock) })
	self := uint64(routine.Goid())
	if st.owner.Load() == self {
		st.count++
		return
	}
	st.mu.Lock()
	st.owner.Store(self)
	st.count = 1
}

//go:linkname _c2go_file_trylock
func _c2go_file_trylock(idp *uint64) int32 {
	st := fileLockTab.lazyInit(idp, func() *fileLock { return new(fileLock) })
	self := uint64(routine.Goid())
	if st.owner.Load() == self {
		st.count++
		return 0
	}
	if st.mu.TryLock() {
		st.owner.Store(self)
		st.count = 1
		return 0
	}
	return 1 // held elsewhere (ftrylockfile: nonzero)
}

//go:linkname _c2go_file_unlock
func _c2go_file_unlock(idp *uint64) {
	st := fileLockTab.get(atomic.LoadUint64(idp))
	if st == nil {
		return
	}
	if st.count--; st.count == 0 {
		st.owner.Store(0)
		st.mu.Unlock()
	}
}

//go:linkname _c2go_file_lock_drop
func _c2go_file_lock_drop(idp *uint64) {
	fileLockTab.free(atomic.SwapUint64(idp, 0))
}

// oflMu guards the open-file list (source/stdio.c's ofl_head) — separate from
// the per-FILE locks: fflush(NULL) holds it while taking each file's own lock.
var oflMu sync.Mutex

//go:linkname _c2go_ofl_lock
func _c2go_ofl_lock() { oflMu.Lock() }

//go:linkname _c2go_ofl_unlock
func _c2go_ofl_unlock() { oflMu.Unlock() }

// __c2go_runtime_rand exposes the Go runtime PRNG to C. It is NOT a libc function
// moved into Go — it only surfaces a runtime primitive the ported C cannot reach
// on its own: tmpfile(3) (source/stdio.c) keeps musl's __randname/tmpfile logic
// in C, but musl derives the temp-name entropy from clock_gettime + the thread
// id, neither of which this port has, so it reuses runtime.rand (the same source
// os.CreateTemp uses, os/tempfile.go).
//
//go:linkname runtime_rand runtime.rand
func runtime_rand() uint64

//go:linkname __c2go_runtime_rand
func __c2go_runtime_rand() uint64 { return runtime_rand() }
