package libc

import (
	"runtime"
	"sync"
	"testing"
	"unsafe"
)

func TestMallocBasic(t *testing.T) {
	p := Malloc(64)
	if p == nil {
		t.Fatal("malloc returned nil")
	}
	b := unsafe.Slice((*byte)(p), 64)
	for i := range b {
		b[i] = byte(i)
	}
	for i := range b {
		if b[i] != byte(i) {
			t.Fatalf("byte %d = %d, want %d", i, b[i], byte(i))
		}
	}
	Free(p)
}

func TestMallocZeroAndFreeNil(t *testing.T) {
	// musl semantics (#661): malloc(0) returns a UNIQUE freeable pointer.
	p, q := Malloc(0), Malloc(0)
	if p == nil || q == nil || p == q {
		t.Fatalf("malloc(0) twice = %p %p, want two distinct non-nil pointers", p, q)
	}
	Free(p)
	Free(q)
	Free(nil) // must not crash
}

func TestCallocZeroed(t *testing.T) {
	p := Calloc(16, 8) // 128 bytes
	if p == nil {
		t.Fatal("calloc returned nil")
	}
	b := unsafe.Slice((*byte)(p), 128)
	for i := range b {
		if b[i] != 0 {
			t.Fatalf("calloc byte %d = %d, want 0", i, b[i])
		}
	}
	Free(p)
	// overflow must fail closed, not wrap.
	if Calloc(^uint64(0), 2) != nil {
		t.Fatal("calloc overflow should return nil")
	}
}

func TestReallocPreserves(t *testing.T) {
	p := Malloc(16)
	b := unsafe.Slice((*byte)(p), 16)
	for i := range b {
		b[i] = byte(i + 1)
	}
	q := Realloc(p, 256) // grow -> moves
	if q == nil {
		t.Fatal("realloc returned nil")
	}
	b2 := unsafe.Slice((*byte)(q), 16)
	for i := 0; i < 16; i++ {
		if b2[i] != byte(i+1) {
			t.Fatalf("realloc lost byte %d: %d", i, b2[i])
		}
	}
	// realloc(nil, n) == malloc(n); realloc(p, 0) == free(p) -> nil.
	if r := Realloc(nil, 8); r == nil {
		t.Fatal("realloc(nil, 8) should allocate")
	} else {
		Free(r)
	}
	if r0 := Realloc(q, 0); r0 == nil {
		t.Fatal("realloc(q, 0) should return a fresh minimal allocation (musl, #661)")
	} else {
		Free(r0)
	}
}

// TestMallocStressGC verifies the handle-table rooting: many live C allocations
// survive GC (backing slices retained by the registry + non-moving heap), and
// the free-index recycling stays consistent across interleaved free/reuse.
func TestMallocStressGC(t *testing.T) {
	const n = 3000
	ptrs := make([]unsafe.Pointer, n)
	sizes := make([]int, n)
	for i := 0; i < n; i++ {
		sz := (i%251 + 1) * 8
		sizes[i] = sz
		p := Malloc(uint64(sz))
		if p == nil {
			t.Fatalf("malloc #%d (%d bytes) returned nil", i, sz)
		}
		s := unsafe.Slice((*byte)(p), sz)
		s[0] = byte(i)
		s[sz-1] = byte(i * 3)
		ptrs[i] = p
	}
	runtime.GC()
	// free the evens, keep the odds.
	for i := 0; i < n; i += 2 {
		Free(ptrs[i])
		ptrs[i] = nil
	}
	runtime.GC()
	// odds must still hold their sentinels; fresh allocs must reuse freed space.
	for i := 1; i < n; i += 2 {
		s := unsafe.Slice((*byte)(ptrs[i]), sizes[i])
		if s[0] != byte(i) || s[sizes[i]-1] != byte(i*3) {
			t.Fatalf("alloc #%d corrupted after GC: [%d,%d]", i, s[0], s[sizes[i]-1])
		}
	}
	for i := 1; i < n; i += 2 {
		Free(ptrs[i])
	}
}

// TestMallocConcurrent hammers the allocator from many goroutines to exercise
// the mallocMu-serialised handle table: a broken lock corrupts the registry or
// the free-index list into a crash or a data mismatch. Run under -race.
func TestMallocConcurrent(t *testing.T) {
	const goroutines, iters = 16, 500
	var wg sync.WaitGroup
	for g := 0; g < goroutines; g++ {
		wg.Add(1)
		go func(seed int) {
			defer wg.Done()
			for it := 0; it < iters; it++ {
				sz := ((seed*7+it)%128 + 1) * 8
				p := Malloc(uint64(sz))
				if p == nil {
					t.Errorf("g%d it%d: malloc nil", seed, it)
					return
				}
				s := unsafe.Slice((*byte)(p), sz)
				tag := byte(seed*31 + it)
				for i := range s {
					s[i] = tag
				}
				for i := range s {
					if s[i] != tag {
						t.Errorf("g%d it%d: byte %d = %d, want %d", seed, it, i, s[i], tag)
						return
					}
				}
				Free(p)
			}
		}(g)
	}
	wg.Wait()
}
