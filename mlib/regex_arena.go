// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"unsafe"

	libc "github.com/c2gohq/c2go_libc"
	"github.com/timandy/routine"
)

// regexArena is the ownership edge for a managed TRE instance. TRE's internal
// records stay byte-for-byte compatible with musl and may contain arbitrary
// scalar data, so scanning every word conservatively would be unsafe. Instead,
// every no-scan gc_malloc block is rooted explicitly here; pointers inside TRE
// blocks are navigation edges only. The arena becomes immutable after regcomp
// succeeds and is reclaimed as one object graph when regfree clears the carrier.
type regexArena struct {
	blocks map[unsafe.Pointer]uint64
}

// The vendored TRE allocator API has no context parameter. Public mlib wrappers
// install the correct arena around each synchronous engine call. This is
// goroutine-local (not a process-global allocator table), so independent regex
// compiles and matches do not contend. regexec uses a fresh temporary arena;
// its compiled TNFA remains rooted by regex_t.__c2go_arena.
var regexArenaLocal = routine.NewThreadLocal[*regexArena]()

//go:linkname regexArenaNew
func regexArenaNew() unsafe.Pointer {
	return unsafe.Pointer(&regexArena{blocks: make(map[unsafe.Pointer]uint64)})
}

//go:linkname regexArenaEnter
func regexArenaEnter(pointer unsafe.Pointer) unsafe.Pointer {
	previous := regexArenaLocal.Get()
	regexArenaLocal.Set((*regexArena)(pointer))
	return unsafe.Pointer(previous)
}

//go:linkname regexArenaLeave
func regexArenaLeave(previous unsafe.Pointer) {
	if previous == nil {
		regexArenaLocal.Remove()
		return
	}
	regexArenaLocal.Set((*regexArena)(previous))
}

func currentRegexArena() *regexArena {
	return regexArenaLocal.Get()
}

//go:linkname regexMalloc
func regexMalloc(size uint64) unsafe.Pointer {
	arena := currentRegexArena()
	if arena == nil {
		return nil
	}
	if size == 0 {
		size = 1
	}
	pointer := libc.GCMalloc(nil, size)
	if pointer != nil {
		arena.blocks[pointer] = size
	}
	return pointer
}

//go:linkname regexCalloc
func regexCalloc(count, size uint64) unsafe.Pointer {
	if count != 0 && size > ^uint64(0)/count {
		return nil
	}
	// GCMalloc always zeroes the returned storage.
	return regexMalloc(count * size)
}

//go:linkname regexRealloc
func regexRealloc(pointer unsafe.Pointer, size uint64) unsafe.Pointer {
	if pointer == nil {
		return regexMalloc(size)
	}
	arena := currentRegexArena()
	if arena == nil {
		return nil
	}
	oldSize, ok := arena.blocks[pointer]
	if !ok {
		return nil
	}
	if size == 0 {
		delete(arena.blocks, pointer)
		return regexMalloc(0)
	}
	replacement := regexMalloc(size)
	if replacement == nil {
		return nil
	}
	copySize := oldSize
	if size < copySize {
		copySize = size
	}
	copy(unsafe.Slice((*byte)(replacement), int(copySize)),
		unsafe.Slice((*byte)(pointer), int(copySize)))
	delete(arena.blocks, pointer)
	return replacement
}

//go:linkname regexFree
func regexFree(pointer unsafe.Pointer) {
	if pointer == nil {
		return
	}
	if arena := currentRegexArena(); arena != nil {
		delete(arena.blocks, pointer)
	}
}
