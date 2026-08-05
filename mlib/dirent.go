// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"sync/atomic"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixdir"
)

func dirCString(pointer *byte) string {
	if pointer == nil {
		return ""
	}
	length := 0
	for *(*byte)(unsafe.Add(unsafe.Pointer(pointer), length)) != 0 {
		length++
	}
	return unsafe.String(pointer, length)
}

func dirState(pointer unsafe.Pointer) *posixdir.Stream {
	return (*posixdir.Stream)(pointer)
}

func storeDirState(out unsafe.Pointer, stream *posixdir.Stream) {
	(*atomic.Pointer[posixdir.Stream])(out).Store(stream)
}

//go:linkname DirOpen
func DirOpen(path *byte, out unsafe.Pointer) int32 {
	storeDirState(out, nil)
	stream, err := posixdir.Open(dirCString(path))
	if err != nil {
		return posixdir.Errno(err)
	}
	storeDirState(out, stream)
	return 0
}

//go:linkname DirRead
func DirRead(state, entry unsafe.Pointer) int32 {
	stream := dirState(state)
	if stream == nil {
		return -errEBADF
	}
	ok, err := stream.Read((*posixdir.Dirent)(entry))
	if err != nil {
		return -posixdir.Errno(err)
	}
	if !ok {
		return 0
	}
	return 1
}

//go:linkname DirClose
func DirClose(state unsafe.Pointer) int32 {
	stream := dirState(state)
	if stream == nil {
		return -errEBADF
	}
	if err := stream.Close(); err != nil {
		return -posixdir.Errno(err)
	}
	return 0
}

//go:linkname DirRewind
func DirRewind(state unsafe.Pointer) int32 {
	stream := dirState(state)
	if stream == nil {
		return -errEBADF
	}
	if err := stream.Rewind(); err != nil {
		return -posixdir.Errno(err)
	}
	return 0
}
