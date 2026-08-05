// SPDX-License-Identifier: AGPL-3.0-only

// Package posixdir contains the Go-owned state and stateless record filling
// shared by libc's handle-based DIR carrier and mlib's direct-pointer carrier.
// It deliberately knows nothing about C carrier layout, handle tables, errno
// storage, or allocation policy.
package posixdir

import (
	"errors"
	"io"
	"os"
	"syscall"
)

// Dirent mirrors c2go-libc's C struct dirent byte-for-byte on every supported
// 64-bit target:
//
//	ino_t d_ino @0, off_t d_off @8, unsigned short d_reclen @16,
//	unsigned char d_type @18, char d_name[256] @19 (size 280, align 8).
//
// It contains no managed pointers and can therefore be filled through either
// carrier layer's C buffer.
type Dirent struct {
	Ino    uint64
	Off    int64
	Reclen uint16
	Type   uint8
	Name   [256]byte
}

// Stream is the Go-owned state behind one directory stream. Root libc keeps a
// *Stream alive through a handle table; mlib keeps it alive through a direct,
// GC-visible pointer in its managed DIR record.
type Stream struct {
	f       *os.File
	path    string
	pending []os.DirEntry
	off     int64
	dot     uint8
}

// Errno extracts a native errno from an os.PathError or another wrapped
// syscall.Errno. This preserves the existing libc bridge behavior.
func Errno(err error) int32 {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		return int32(errno)
	}
	return int32(syscall.EIO)
}

// Open opens and validates a directory by path. This function owns the newly
// opened file and closes it again if validation fails.
func Open(path string) (*Stream, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	stream, err := NewFile(f, path)
	if err != nil {
		_ = f.Close()
		return nil, err
	}
	return stream, nil
}

// NewFile validates an already-open file and, on success, transfers its
// lifetime to a Stream. On failure ownership remains with the caller, matching
// fdopendir's requirement that a failed conversion must not consume the fd.
func NewFile(f *os.File, path string) (*Stream, error) {
	if f == nil {
		return nil, syscall.EBADF
	}
	info, err := f.Stat()
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		return nil, syscall.ENOTDIR
	}
	return &Stream{f: f, path: path}, nil
}

// Read fills one C-layout directory entry. ok=false, err=nil is end of
// directory; a non-nil error is reported by the carrier layer.
func (s *Stream) Read(out *Dirent) (ok bool, err error) {
	if s == nil || s.f == nil {
		return false, syscall.EBADF
	}
	// os.ReadDir omits "." and ".."; POSIX readdir yields them first.
	if s.dot < 2 {
		name := "."
		if s.dot == 1 {
			name = ".."
		}
		s.dot++
		s.off++
		fillRaw(out, name, 4 /* DT_DIR */, uint64(s.off))
		return true, nil
	}
	if len(s.pending) == 0 {
		entries, readErr := s.f.ReadDir(64)
		if len(entries) == 0 {
			if readErr == nil || readErr == io.EOF {
				return false, nil
			}
			return false, readErr
		}
		s.pending = entries
	}
	entry := s.pending[0]
	s.pending = s.pending[1:]
	s.off++
	fill(out, entry, uint64(s.off))
	return true, nil
}

// Close releases the OS directory resource. The carrier itself is released by
// its own ownership model (handle retirement for libc, GC reachability for
// mlib).
func (s *Stream) Close() error {
	if s == nil || s.f == nil {
		return syscall.EBADF
	}
	f := s.f
	s.f = nil
	s.pending = nil
	return f.Close()
}

// Rewind resets enumeration by reopening the original path. A stream created
// from an fd whose path could not be recovered reports the resulting open
// error, matching the previous bridge behavior.
func (s *Stream) Rewind() error {
	if s == nil || s.f == nil {
		return syscall.EBADF
	}
	next, err := os.Open(s.path)
	if err != nil {
		return err
	}
	_ = s.f.Close()
	s.f = next
	s.pending = nil
	s.off = 0
	s.dot = 0
	return nil
}

// FD returns the underlying platform descriptor. Callers expose it only on
// Unix; on Windows os.File.Fd is a HANDLE rather than a CRT descriptor.
func (s *Stream) FD() (uintptr, error) {
	if s == nil || s.f == nil {
		return 0, syscall.EBADF
	}
	return s.f.Fd(), nil
}

func fill(out *Dirent, entry os.DirEntry, sequence uint64) {
	ino := sequence
	if info, err := entry.Info(); err == nil {
		if real := inode(info); real != 0 {
			ino = real
		}
	}
	fillRaw(out, entry.Name(), direntType(entry.Type()), ino)
	out.Off = int64(sequence)
}

func fillRaw(out *Dirent, name string, typ uint8, ino uint64) {
	out.Ino = ino
	out.Off = int64(ino)
	out.Type = typ
	if len(name) > 255 {
		name = name[:255]
	}
	copy(out.Name[:], name)
	out.Name[len(name)] = 0
	out.Reclen = uint16(19 + len(name) + 1)
}

func direntType(mode os.FileMode) uint8 {
	switch {
	case mode&os.ModeDir != 0:
		return 4 // DT_DIR
	case mode&os.ModeSymlink != 0:
		return 10 // DT_LNK
	case mode&os.ModeNamedPipe != 0:
		return 1 // DT_FIFO
	case mode&os.ModeSocket != 0:
		return 12 // DT_SOCK
	case mode&os.ModeCharDevice != 0:
		return 2 // DT_CHR
	case mode&os.ModeDevice != 0:
		return 6 // DT_BLK
	case mode.IsRegular():
		return 8 // DT_REG
	default:
		return 0 // DT_UNKNOWN
	}
}
