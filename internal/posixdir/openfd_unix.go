// SPDX-License-Identifier: AGPL-3.0-only

//go:build unix

package posixdir

import (
	"os"
	"syscall"
)

// OpenFD adopts an already-open directory descriptor on success. When
// validation fails the descriptor remains the caller's responsibility, as
// required by fdopendir.
func OpenFD(fd int32) (*Stream, error) {
	if fd < 0 {
		return nil, syscall.EBADF
	}
	// Validate before os.NewFile. NewFile installs a finalizer that owns the
	// descriptor, so constructing it on an error path could eventually close a
	// descriptor that POSIX says fdopendir has not consumed.
	var stat syscall.Stat_t
	if err := syscall.Fstat(int(fd), &stat); err != nil {
		return nil, err
	}
	if stat.Mode&syscall.S_IFMT != syscall.S_IFDIR {
		return nil, syscall.ENOTDIR
	}
	path := fdPath(fd)
	file := os.NewFile(uintptr(fd), path)
	if file == nil {
		return nil, syscall.EBADF
	}
	return &Stream{f: file, path: path}, nil
}
