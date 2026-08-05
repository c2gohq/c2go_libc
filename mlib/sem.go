// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"sync/atomic"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixsync"
)

// semSlot views the managed C carrier's first word as a direct Go pointer.
// Store uses Go's pointer write barrier; C2Go's managed metadata makes the same
// word visible to stack, global, and gc_malloc heap scanning.
func semSlot(p unsafe.Pointer) *atomic.Pointer[posixsync.Semaphore] {
	return (*atomic.Pointer[posixsync.Semaphore])(p)
}

func semResultCode(result posixsync.SemResult) int32 {
	switch result {
	case posixsync.SemOK:
		return 0
	case posixsync.SemWouldBlock:
		return errEAGAIN
	case posixsync.SemTimedOut:
		return errETIMEDOUT
	case posixsync.SemOverflow:
		return errEOVERFLOW
	default:
		return errEINVAL
	}
}

//go:linkname SemInit
func SemInit(s unsafe.Pointer, pshared int32, value uint32) int32 {
	st, result := posixsync.NewSemaphore(value)
	if result != posixsync.SemOK {
		return semResultCode(result)
	}
	semSlot(s).Store(st)
	return 0
}

//go:linkname SemDestroy
func SemDestroy(s unsafe.Pointer) int32 {
	semSlot(s).Store(nil)
	return 0
}

//go:linkname SemWait
func SemWait(s unsafe.Pointer) int32 { return semWait(s, nil) }

//go:linkname SemTimedwait
func SemTimedwait(s, abstime unsafe.Pointer) int32 { return semWait(s, abstime) }

func semWait(s, abstime unsafe.Pointer) int32 {
	if abstime != nil && !posixsync.TimespecValid(abstime) {
		return errEINVAL
	}
	st := semSlot(s).Load()
	if st == nil {
		return errEINVAL
	}
	return semResultCode(st.Wait(abstime))
}

//go:linkname SemTrywait
func SemTrywait(s unsafe.Pointer) int32 {
	st := semSlot(s).Load()
	if st == nil {
		return errEINVAL
	}
	return semResultCode(st.TryWait())
}

//go:linkname SemPost
func SemPost(s unsafe.Pointer) int32 {
	st := semSlot(s).Load()
	if st == nil {
		return errEINVAL
	}
	return semResultCode(st.Post())
}

//go:linkname SemGetvalue
func SemGetvalue(s, out unsafe.Pointer) int32 {
	st := semSlot(s).Load()
	if st == nil {
		return errEINVAL
	}
	*(*int32)(out) = st.Value()
	return 0
}
