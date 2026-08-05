// SPDX-License-Identifier: AGPL-3.0-only

// Managed pthread mutex, condition-variable, and rwlock carriers.
//
// Each carrier's first word is a direct, GC-visible pointer to the shared
// internal state. The root libc exposes the same behavior through handle IDs;
// only state ownership and resolution differ here.
package mlib

import (
	"sync/atomic"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixsync"
)

const (
	mutexNormal     = int32(posixsync.MutexNormal)
	mutexRecursive  = int32(posixsync.MutexRecursive)
	mutexErrorcheck = int32(posixsync.MutexErrorcheck)
)

func pthreadResultCode(result posixsync.SyncResult) int32 {
	switch result {
	case posixsync.SyncOK:
		return 0
	case posixsync.SyncBusy:
		return errEBUSY
	case posixsync.SyncPermission:
		return errEPERM
	case posixsync.SyncDeadlock:
		return errEDEADLK
	case posixsync.SyncTimedOut:
		return errETIMEDOUT
	default:
		return errEINVAL
	}
}

func mutexSlot(p unsafe.Pointer) *atomic.Pointer[posixsync.Mutex] {
	return (*atomic.Pointer[posixsync.Mutex])(p)
}

func loadOrInitMutex(p unsafe.Pointer) *posixsync.Mutex {
	slot := mutexSlot(p)
	if state := slot.Load(); state != nil {
		return state
	}
	state := posixsync.NewMutex(posixsync.MutexNormal)
	if slot.CompareAndSwap(nil, state) {
		return state
	}
	return slot.Load()
}

//go:linkname PthreadMutexInit
func PthreadMutexInit(m, attr unsafe.Pointer) int32 {
	kind := posixsync.MutexNormal
	if attr != nil {
		kind = posixsync.MutexKind(*(*int32)(attr))
	}
	if !posixsync.ValidMutexKind(kind) {
		return errEINVAL
	}
	if kind == posixsync.MutexNormal {
		mutexSlot(m).Store(nil)
	} else {
		mutexSlot(m).Store(posixsync.NewMutex(kind))
	}
	return 0
}

//go:linkname PthreadMutexDestroy
func PthreadMutexDestroy(m unsafe.Pointer) int32 {
	mutexSlot(m).Store(nil)
	return 0
}

//go:linkname PthreadMutexLock
func PthreadMutexLock(m unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitMutex(m).Lock())
}

//go:linkname PthreadMutexTryLock
func PthreadMutexTryLock(m unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitMutex(m).TryLock())
}

//go:linkname PthreadMutexTimedlock
func PthreadMutexTimedlock(m, abstime unsafe.Pointer) int32 {
	if abstime == nil || !posixsync.TimespecValid(abstime) {
		return errEINVAL
	}
	return pthreadResultCode(loadOrInitMutex(m).TimedLock(abstime))
}

//go:linkname PthreadMutexUnlock
func PthreadMutexUnlock(m unsafe.Pointer) int32 {
	state := mutexSlot(m).Load()
	if state == nil {
		return errEPERM
	}
	return pthreadResultCode(state.Unlock())
}

func rwlockSlot(p unsafe.Pointer) *atomic.Pointer[posixsync.RWLock] {
	return (*atomic.Pointer[posixsync.RWLock])(p)
}

func loadOrInitRWLock(p unsafe.Pointer) *posixsync.RWLock {
	slot := rwlockSlot(p)
	if state := slot.Load(); state != nil {
		return state
	}
	state := posixsync.NewRWLock()
	if slot.CompareAndSwap(nil, state) {
		return state
	}
	return slot.Load()
}

//go:linkname PthreadRWLockInit
func PthreadRWLockInit(rw, attr unsafe.Pointer) int32 {
	rwlockSlot(rw).Store(nil)
	return 0
}

//go:linkname PthreadRWLockDestroy
func PthreadRWLockDestroy(rw unsafe.Pointer) int32 {
	rwlockSlot(rw).Store(nil)
	return 0
}

//go:linkname PthreadRWLockRdlock
func PthreadRWLockRdlock(rw unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitRWLock(rw).ReadLock())
}

//go:linkname PthreadRWLockWrlock
func PthreadRWLockWrlock(rw unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitRWLock(rw).WriteLock())
}

//go:linkname PthreadRwlockTryrdlock
func PthreadRwlockTryrdlock(rw unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitRWLock(rw).TryReadLock())
}

//go:linkname PthreadRwlockTrywrlock
func PthreadRwlockTrywrlock(rw unsafe.Pointer) int32 {
	return pthreadResultCode(loadOrInitRWLock(rw).TryWriteLock())
}

//go:linkname PthreadRWLockUnlock
func PthreadRWLockUnlock(rw unsafe.Pointer) int32 {
	state := rwlockSlot(rw).Load()
	if state == nil {
		return errEPERM
	}
	return pthreadResultCode(state.Unlock())
}

func condSlot(p unsafe.Pointer) *atomic.Pointer[posixsync.Cond] {
	return (*atomic.Pointer[posixsync.Cond])(p)
}

func loadOrInitCond(p unsafe.Pointer) *posixsync.Cond {
	slot := condSlot(p)
	if state := slot.Load(); state != nil {
		return state
	}
	state := posixsync.NewCond()
	if slot.CompareAndSwap(nil, state) {
		return state
	}
	return slot.Load()
}

//go:linkname PthreadCondInit
func PthreadCondInit(cond, attr unsafe.Pointer) int32 {
	condSlot(cond).Store(nil)
	return 0
}

//go:linkname PthreadCondDestroy
func PthreadCondDestroy(cond unsafe.Pointer) int32 {
	condSlot(cond).Store(nil)
	return 0
}

//go:linkname PthreadCondWait
func PthreadCondWait(cond, mutex unsafe.Pointer) int32 {
	return pthreadCondWait(cond, mutex, nil)
}

//go:linkname PthreadCondTimedwait
func PthreadCondTimedwait(cond, mutex, abstime unsafe.Pointer) int32 {
	return pthreadCondWait(cond, mutex, abstime)
}

func pthreadCondWait(cond, mutex, abstime unsafe.Pointer) int32 {
	if abstime != nil && !posixsync.TimespecValid(abstime) {
		return errEINVAL
	}
	condState := loadOrInitCond(cond)
	mutexState := mutexSlot(mutex).Load()
	if mutexState == nil {
		return errEINVAL
	}
	return pthreadResultCode(condState.Wait(mutexState, abstime))
}

//go:linkname PthreadCondSignal
func PthreadCondSignal(cond unsafe.Pointer) int32 {
	if state := condSlot(cond).Load(); state != nil {
		state.Signal()
	}
	return 0
}

//go:linkname PthreadCondBroadcast
func PthreadCondBroadcast(cond unsafe.Pointer) int32 {
	if state := condSlot(cond).Load(); state != nil {
		state.Broadcast()
	}
	return 0
}

// Attribute records contain no managed pointers, but namespaced mlib callers
// still get a complete, consistently-prefixed lifecycle surface.

//go:linkname PthreadMutexAttrInit
func PthreadMutexAttrInit(attr unsafe.Pointer) int32 {
	*(*[24]byte)(attr) = [24]byte{}
	return 0
}

//go:linkname PthreadMutexAttrSetType
func PthreadMutexAttrSetType(attr unsafe.Pointer, kind int32) int32 {
	if attr == nil || !posixsync.ValidMutexKind(posixsync.MutexKind(kind)) {
		return errEINVAL
	}
	*(*int32)(attr) = kind
	return 0
}

//go:linkname PthreadMutexAttrGetType
func PthreadMutexAttrGetType(attr, out unsafe.Pointer) int32 {
	if attr == nil || out == nil {
		return errEINVAL
	}
	*(*int32)(out) = *(*int32)(attr)
	return 0
}

//go:linkname PthreadMutexAttrDestroy
func PthreadMutexAttrDestroy(attr unsafe.Pointer) int32 { return 0 }

//go:linkname PthreadCondAttrInit
func PthreadCondAttrInit(attr unsafe.Pointer) int32 {
	*(*[24]byte)(attr) = [24]byte{}
	return 0
}

//go:linkname PthreadCondAttrDestroy
func PthreadCondAttrDestroy(attr unsafe.Pointer) int32 { return 0 }

//go:linkname PthreadRWLockAttrInit
func PthreadRWLockAttrInit(attr unsafe.Pointer) int32 {
	*(*[24]byte)(attr) = [24]byte{}
	return 0
}

//go:linkname PthreadRWLockAttrDestroy
func PthreadRWLockAttrDestroy(attr unsafe.Pointer) int32 { return 0 }
