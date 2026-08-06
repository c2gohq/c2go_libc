// thread.go — pthread lifecycle: create / join / detach / self / equal / yield.
//
// A pthread maps to a goroutine. Root pthread_t is a handle id into threadTab;
// managed mlib pthread_t is the direct Go state pointer. The start routine is a
// c2go C function pointer, so it is invoked IN C by the
// __c2go_pthread_run trampoline
// (source/pthread.c, reached through the c2go-bind-generated C2goPthreadRun) —
// the "call the fp in C" pattern of qsort.c — while only the `go` spawn is here.
//
// attr setters + pthread_once live here too; pthread_key (thread-specific data,
// whose destructors this file's finish defer runs on thread exit) is in
// pthread_key.go. Portable Go; errno constants come from errno_{unix,windows}.go.

package libc

import (
	"runtime"
	"sync/atomic"
	"unsafe"

	"github.com/timandy/routine"
)

// threadState is the Go object behind either pthread carrier. Root libc roots
// it through threadTab and publishes the table id; mlib publishes the direct
// pointer in GC-visible C storage. detached/reaped are atomic because join,
// detach and the finishing goroutine all race on them; retval is published by
// the goroutine before it closes done, so a joiner that has observed done sees
// the final retval. Whichever path successfully reaps the thread also retires
// retval after either publishing it to the joiner or discarding it for detach.
type threadState struct {
	done     chan struct{}  // closed when the start routine returns
	retval   unsafe.Pointer // the void* the start routine returned
	detached atomic.Bool
	reaped   atomic.Bool // CAS: exactly one of join / detach / finish frees the slot
	id       atomic.Uint64
	started  bool // true when pthread_create installed the finish defer
}

var threadTab handleTable[threadState]

// curThread names the running goroutine's own thread record (for pthread_self).
var curThread = routine.NewThreadLocal[*threadState]()

// installThreadHandle lazily gives a direct-managed thread a root-libc identity
// if code deliberately crosses into the root pthread_self surface. Ordinary
// mlib use never calls it and therefore never occupies threadTab.
func installThreadHandle(st *threadState) uint64 {
	if id := st.id.Load(); id != 0 {
		return id
	}
	id := threadTab.alloc(st)
	if st.id.CompareAndSwap(0, id) {
		return id
	}
	threadTab.free(id)
	return st.id.Load()
}

// reap retires a state exactly once, whichever of finish/join/detach wins. A
// root carrier also frees its table slot; a direct mlib carrier has id zero.
func reap(st *threadState) bool {
	if st.reaped.CompareAndSwap(false, true) {
		if id := st.id.Swap(0); id != 0 {
			threadTab.free(id)
		}
		return true
	}
	return false
}

// reapDetached retires a detached thread and its retained result together.
// Only the successful reaper may touch retval: this keeps a racing join/detach
// pair from reading and clearing the managed root concurrently.
func reapDetached(st *threadState) bool {
	if !reap(st) {
		return false
	}
	st.retval = nil
	return true
}

func chanClosed(ch chan struct{}) bool {
	select {
	case <-ch:
		return true
	default:
		return false
	}
}

func newThreadState(attr unsafe.Pointer) *threadState {
	// pthread_attr_t._detachstate is the leading int; PTHREAD_CREATE_DETACHED == 1.
	detached := attr != nil && *(*int32)(attr) == 1
	st := &threadState{done: make(chan struct{}), started: true}
	st.detached.Store(detached)
	return st
}

func startThread(st *threadState, start, arg unsafe.Pointer) {
	go func() {
		curThread.Set(st)
		// Finalize via defer so BOTH exits reach it: a normal return from the
		// start routine, and pthread_exit (which stashes retval into st and
		// unwinds this goroutine via runtime.Goexit — the defer still runs).
		defer func() {
			runKeyDestructors() // POSIX: run this thread's TSD destructors on exit
			close(st.done)
			if st.detached.Load() {
				reapDetached(st)
			}
		}()
		// Run the start routine in C (it is a c2go fp). A normal return yields
		// its void* result; pthread_exit sets st.retval instead and never returns.
		st.retval = C2goPthreadRun(uintptr(start), arg)
	}()
}

//go:linkname PthreadCreate
func PthreadCreate(thread, attr, start, arg unsafe.Pointer) int32 {
	st := newThreadState(attr)
	id := installThreadHandle(st)
	*(*uint64)(thread) = id
	startThread(st, start, arg)
	return 0
}

// __c2go_pthread_create_managed publishes the direct thread state through a Go
// pointer store. Its output slot and argument are managed by mlib's header.
//
//go:linkname __c2go_pthread_create_managed
func __c2go_pthread_create_managed(thread, attr, start, arg unsafe.Pointer) int32 {
	if thread == nil {
		return errEINVAL
	}
	st := newThreadState(attr)
	*(*unsafe.Pointer)(thread) = unsafe.Pointer(st)
	startThread(st, start, arg)
	return 0
}

func joinThread(st *threadState, retval unsafe.Pointer) int32 {
	if st == nil {
		return errESRCH
	}
	if st.detached.Load() {
		return errEINVAL // a detached thread is not joinable
	}
	<-st.done
	if !reap(st) {
		return errESRCH
	}
	result := st.retval
	st.retval = nil
	if retval != nil {
		*(*unsafe.Pointer)(retval) = result
	}
	return 0
}

//go:linkname PthreadJoin
func PthreadJoin(t uint64, retval unsafe.Pointer) int32 {
	return joinThread(threadTab.get(t), retval)
}

//go:linkname __c2go_pthread_join_managed
func __c2go_pthread_join_managed(thread, retval unsafe.Pointer) int32 {
	return joinThread((*threadState)(thread), retval)
}

//go:linkname PthreadExit
func PthreadExit(retval unsafe.Pointer) {
	// Terminate the calling thread, publishing retval to a future joiner. The
	// caller is deep inside the start routine's C frames; runtime.Goexit unwinds
	// this goroutine (running the PthreadCreate defer that closes done / reaps),
	// so retval must be stashed BEFORE unwinding. curThread is nil only if a
	// non-pthread goroutine calls this — Goexit still terminates it (no retval to
	// hand out, which matches: nothing joins a goroutine we do not track).
	if st := curThread.Get(); st != nil {
		st.retval = retval
	}
	runtime.Goexit()
}

func detachThread(st *threadState) int32 {
	if st == nil {
		return errESRCH
	}
	if st.reaped.Load() {
		return errESRCH
	}
	st.detached.Store(true)
	if chanClosed(st.done) {
		reapDetached(st) // already finished: reclaim result and state now
	}
	return 0
}

//go:linkname PthreadDetach
func PthreadDetach(t uint64) int32 { return detachThread(threadTab.get(t)) }

//go:linkname __c2go_pthread_detach_managed
func __c2go_pthread_detach_managed(thread unsafe.Pointer) int32 {
	return detachThread((*threadState)(thread))
}

func currentThreadState() *threadState {
	if st := curThread.Get(); st != nil {
		return st
	}
	st := &threadState{done: make(chan struct{})}
	st.detached.Store(true)
	curThread.Set(st)
	return st
}

//go:linkname PthreadSelf
func PthreadSelf() uint64 {
	st := currentThreadState()
	id := installThreadHandle(st)
	if st.started {
		return id
	}
	// A goroutine not created via pthread_create (the main thread, or a Go-native
	// one): register a detached self-record so it has a stable, comparable id.
	// No pthread finish defer will ever reap this record, so without a death hook
	// every goroutine that calls pthread_self() would leak its threadTab slot. Arm
	// a GC-cleanup on a per-goroutine sentinel (held only in g.labels, so it dies
	// with the goroutine) to free a lazily installed slot then.
	ts := glsLookup()
	if ts.threadDeath == nil {
		ts.threadDeath = new(byte)
		runtime.AddCleanup(ts.threadDeath, reapSelfRecord, st)
	}
	return id
}

// reapSelfRecord frees a pthread_self() self-record's threadTab slot when its
// goroutine dies (armed by PthreadSelf via runtime.AddCleanup).
func reapSelfRecord(st *threadState) {
	if id := st.id.Swap(0); id != 0 {
		threadTab.free(id)
	}
}

//go:linkname __c2go_pthread_self_managed
func __c2go_pthread_self_managed() unsafe.Pointer {
	return unsafe.Pointer(currentThreadState())
}

//go:linkname PthreadEqual
func PthreadEqual(a, b uint64) int32 {
	if a == b {
		return 1
	}
	return 0
}

//go:linkname __c2go_pthread_equal_managed
func __c2go_pthread_equal_managed(a, b unsafe.Pointer) int32 {
	if a == b {
		return 1
	}
	return 0
}

//go:linkname PthreadYield
func PthreadYield() int32 {
	runtime.Gosched()
	return 0
}

// ─────────────────────────────── attr ───────────────────────────────
//
// pthread_attr_t is plain C memory whose leading int is _detachstate (the only
// attribute pthread_create honors). A requested stack size is accepted but not
// enforced — a goroutine's stack starts small and auto-grows, so it already
// supplies at least the requested amount; there is no fixed-size stack to set.

//go:linkname PthreadAttrInit
func PthreadAttrInit(attr unsafe.Pointer) int32 {
	*(*int32)(attr) = 0 // PTHREAD_CREATE_JOINABLE
	return 0
}

//go:linkname PthreadAttrDestroy
func PthreadAttrDestroy(attr unsafe.Pointer) int32 { return 0 }

//go:linkname PthreadAttrSetDetachState
func PthreadAttrSetDetachState(attr unsafe.Pointer, state int32) int32 {
	if state != 0 && state != 1 { // JOINABLE / DETACHED
		return errEINVAL
	}
	*(*int32)(attr) = state
	return 0
}

//go:linkname PthreadAttrSetStackSize
func PthreadAttrSetStackSize(attr unsafe.Pointer, size uintptr) int32 {
	// Advisory: goroutines auto-grow, so any request is satisfied by construction.
	return 0
}

// ─────────────────────────────── once ───────────────────────────────

//go:linkname PthreadOnce
func PthreadOnce(once, initRoutine unsafe.Pointer) int32 {
	// pthread_once_t is { int _done; int _pad; size_t _state; }; _state (offset 8)
	// is the CAS token that elects the single initializer, _done (offset 0) is the
	// flag late callers wait on. _state is size_t, not unsigned long, so this
	// 8-byte uintptr CAS stays inside the struct on Windows LLP64 (see pthread.h).
	done := (*int32)(once)
	state := (*uintptr)(unsafe.Add(once, 8))
	if atomic.LoadInt32(done) != 0 {
		return 0 // fast path: init already completed
	}
	if atomic.CompareAndSwapUintptr(state, 0, 1) {
		C2goPthreadRunVoid(uintptr(initRoutine)) // the winner runs init in C
		atomic.StoreInt32(done, 1)
		return 0
	}
	for atomic.LoadInt32(done) == 0 { // losers spin until the winner flips done
		runtime.Gosched()
	}
	return 0
}
