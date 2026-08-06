// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"testing"
	"unsafe"
)

func TestRegexArenaAllocator(t *testing.T) {
	arenaPointer := regexArenaNew()
	if arenaPointer == nil {
		t.Fatal("regexArenaNew returned nil")
	}
	previous := regexArenaEnter(arenaPointer)
	defer regexArenaLeave(previous)

	arena := (*regexArena)(arenaPointer)
	pointer := regexCalloc(4, 8)
	if pointer == nil {
		t.Fatal("regexCalloc returned nil")
	}
	if len(arena.blocks) != 1 || arena.blocks[pointer] != 32 {
		t.Fatalf("calloc roots = %#v, want one 32-byte block", arena.blocks)
	}
	bytes := unsafe.Slice((*byte)(pointer), 32)
	for index, value := range bytes {
		if value != 0 {
			t.Fatalf("calloc byte %d = %d, want zero", index, value)
		}
		bytes[index] = byte(index + 1)
	}

	replacement := regexRealloc(pointer, 64)
	if replacement == nil {
		t.Fatal("regexRealloc returned nil")
	}
	if len(arena.blocks) != 1 || arena.blocks[replacement] != 64 {
		t.Fatalf("realloc roots = %#v, want one 64-byte block", arena.blocks)
	}
	for index, value := range unsafe.Slice((*byte)(replacement), 32) {
		if want := byte(index + 1); value != want {
			t.Fatalf("realloc byte %d = %d, want %d", index, value, want)
		}
	}

	regexFree(replacement)
	if len(arena.blocks) != 0 {
		t.Fatalf("free retained roots: %#v", arena.blocks)
	}
}

func TestRegexArenaRejectsCallocOverflow(t *testing.T) {
	arenaPointer := regexArenaNew()
	previous := regexArenaEnter(arenaPointer)
	defer regexArenaLeave(previous)

	if pointer := regexCalloc(^uint64(0), 2); pointer != nil {
		t.Fatalf("overflowing calloc returned %p", pointer)
	}
	if blocks := (*regexArena)(arenaPointer).blocks; len(blocks) != 0 {
		t.Fatalf("overflowing calloc retained roots: %#v", blocks)
	}
}
