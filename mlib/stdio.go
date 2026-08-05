// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"sync"
	"sync/atomic"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixstdio"
)

func fileLockSlot(slot unsafe.Pointer) *atomic.Pointer[posixstdio.Lock] {
	return (*atomic.Pointer[posixstdio.Lock])(slot)
}

func loadOrCreateFileLock(slot unsafe.Pointer) *posixstdio.Lock {
	p := fileLockSlot(slot)
	if state := p.Load(); state != nil {
		return state
	}
	created := new(posixstdio.Lock)
	if p.CompareAndSwap(nil, created) {
		return created
	}
	return p.Load()
}

//go:linkname FileLock
func FileLock(slot unsafe.Pointer) {
	loadOrCreateFileLock(slot).Lock()
}

//go:linkname FileTryLock
func FileTryLock(slot unsafe.Pointer) int32 {
	if loadOrCreateFileLock(slot).TryLock() {
		return 0
	}
	return 1
}

//go:linkname FileUnlock
func FileUnlock(state unsafe.Pointer) {
	(*posixstdio.Lock)(state).Unlock()
}

var openFileMu sync.Mutex

//go:linkname OpenFileListLock
func OpenFileListLock() { openFileMu.Lock() }

//go:linkname OpenFileListUnlock
func OpenFileListUnlock() { openFileMu.Unlock() }
