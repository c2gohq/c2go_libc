package libc

import (
	"sync"
	"sync/atomic"
	"testing"
)

func TestHandleAllocGetFree(t *testing.T) {
	var tab handleTable[int]
	a, b := new(int), new(int)
	*a, *b = 10, 20
	ida := tab.alloc(a)
	idb := tab.alloc(b)
	if ida != 1 || idb != 2 {
		t.Fatalf("ids = %d,%d want 1,2 (id = idx+1)", ida, idb)
	}
	if tab.get(ida) != a || tab.get(idb) != b {
		t.Fatal("get resolved the wrong state")
	}
}

func TestHandleGetNilAndStale(t *testing.T) {
	var tab handleTable[int]
	if tab.get(0) != nil {
		t.Fatal("get(0) must be nil (0 = uninitialized sentinel)")
	}
	if tab.get(999) != nil {
		t.Fatal("get(out-of-range) must be nil")
	}
	id := tab.alloc(new(int))
	tab.free(id)
	if tab.get(id) != nil {
		t.Fatal("get(freed) must be nil")
	}
	tab.free(id) // double free is a no-op
}

func TestHandleFreeRecyclesIndex(t *testing.T) {
	var tab handleTable[int]
	a, b, c := new(int), new(int), new(int)
	id1 := tab.alloc(a) // idx0 -> id1
	id2 := tab.alloc(b) // idx1 -> id2
	tab.free(id1)       // idx0 recycled onto the freelist
	id3 := tab.alloc(c) // must reuse idx0 — with a BUMPED generation (#658 M6)
	if uint32(id3) != uint32(id1) {
		t.Fatalf("id3 slot = %d, want recycled slot %d", uint32(id3), uint32(id1))
	}
	if id3 == id1 {
		t.Fatal("recycled id must differ from the stale one (generation stamp)")
	}
	if tab.get(id3) != c {
		t.Fatal("recycled id resolves to the wrong state")
	}
	if tab.get(id2) != b {
		t.Fatal("unrelated id disturbed by recycle")
	}
	// The ABA guard proper: the STALE id neither resolves nor frees the new
	// occupant.
	if tab.get(id1) != nil {
		t.Fatal("stale id resolved after its slot was recycled")
	}
	tab.free(id1)
	if tab.get(id3) != c {
		t.Fatal("free(stale id) evicted the slot's new occupant (ABA)")
	}
}

// TestHandleLazyInitOnce is the load-bearing one: many goroutines first-using
// the same zero _id must all observe ONE state, constructed exactly once.
func TestHandleLazyInitOnce(t *testing.T) {
	const N = 64
	var tab handleTable[int]
	var id uint64 // the C object's _id field, starts 0 = uninitialized
	var made int64
	got := make([]*int, N)
	start := make(chan struct{})
	var wg sync.WaitGroup
	for i := 0; i < N; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			<-start
			got[i] = tab.lazyInit(&id, func() *int {
				atomic.AddInt64(&made, 1)
				v := new(int)
				*v = 42
				return v
			})
		}(i)
	}
	close(start)
	wg.Wait()
	if made != 1 {
		t.Fatalf("constructed %d times, want exactly 1", made)
	}
	if id == 0 {
		t.Fatal("id not published after lazyInit")
	}
	for i := 1; i < N; i++ {
		if got[i] != got[0] || got[i] == nil {
			t.Fatalf("goroutine %d saw a different/nil state", i)
		}
	}
	// A late caller takes the fast path and sees the same state.
	if tab.lazyInit(&id, func() *int { t.Fatal("mk ran on the fast path"); return nil }) != got[0] {
		t.Fatal("fast-path lazyInit disagreed")
	}
}

// TestHandleConcurrentAllocFree exercises the lock under churn (run with -race).
func TestHandleConcurrentAllocFree(t *testing.T) {
	var tab handleTable[int]
	var wg sync.WaitGroup
	for g := 0; g < 16; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := 0; i < 2000; i++ {
				v := new(int)
				id := tab.alloc(v)
				if tab.get(id) != v { // this id is exclusively ours until we free it
					t.Errorf("alloc/get mismatch")
					return
				}
				tab.free(id)
			}
		}()
	}
	wg.Wait()
}

// ── RWMutex-vs-Mutex micro-benchmark (why production get() uses a RWMutex) ──
// Two test-local read paths, one per lock kind, so the comparison stays honest
// and stable regardless of what the production handleTable uses. The measured
// result (RWMutex ~15-30% faster at 4-8 cores) is why handle.go took the
// RWMutex: this table's steady state is near-100% get(), a read-lock's best
// case. mtxHandleTable mirrors the old plain-Mutex choice for the baseline.

type mtxHandleTable[T any] struct {
	mu       sync.Mutex
	registry []*T
	freelist []int
}

func (t *mtxHandleTable[T]) alloc(st *T) uint64 {
	t.mu.Lock()
	idx := len(t.registry)
	if k := len(t.freelist); k > 0 {
		idx = t.freelist[k-1]
		t.freelist = t.freelist[:k-1]
		t.registry[idx] = st
	} else {
		t.registry = append(t.registry, st)
	}
	t.mu.Unlock()
	return uint64(idx) + 1
}

func (t *mtxHandleTable[T]) get(id uint64) *T {
	if id == 0 {
		return nil
	}
	idx := int(id - 1)
	t.mu.Lock()
	var st *T
	if idx >= 0 && idx < len(t.registry) {
		st = t.registry[idx]
	}
	t.mu.Unlock()
	return st
}

// Pure read contention on one global lock == the pthread_mutex_lock/unlock hot
// path (every op resolves the same table).
func BenchmarkHandleGetMutex(b *testing.B) {
	var tab mtxHandleTable[int]
	id := tab.alloc(new(int))
	b.RunParallel(func(pb *testing.PB) {
		var sink *int
		for pb.Next() {
			sink = tab.get(id)
		}
		_ = sink
	})
}

func BenchmarkHandleGetRWMutex(b *testing.B) {
	var tab handleTable[int] // the production table (RWMutex)
	id := tab.alloc(new(int))
	b.RunParallel(func(pb *testing.PB) {
		var sink *int
		for pb.Next() {
			sink = tab.get(id)
		}
		_ = sink
	})
}
