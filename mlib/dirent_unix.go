// SPDX-License-Identifier: AGPL-3.0-only

//go:build unix

package mlib

import (
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixdir"
)

//go:linkname DirFD
func DirFD(state unsafe.Pointer) int64 {
	stream := dirState(state)
	if stream == nil {
		return -int64(errEBADF)
	}
	fd, err := stream.FD()
	if err != nil {
		return -int64(posixdir.Errno(err))
	}
	return int64(fd)
}

//go:linkname DirOpenFD
func DirOpenFD(fd int32, out unsafe.Pointer) int32 {
	storeDirState(out, nil)
	stream, err := posixdir.OpenFD(fd)
	if err != nil {
		return posixdir.Errno(err)
	}
	storeDirState(out, stream)
	return 0
}
