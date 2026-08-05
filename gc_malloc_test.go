// SPDX-License-Identifier: AGPL-3.0-only

package libc

import (
	"reflect"
	"runtime"
	"testing"
	"unsafe"
)

type gcMallocPointerRecord struct {
	a unsafe.Pointer
	b unsafe.Pointer
	c unsafe.Pointer
	d unsafe.Pointer
}

func gcMallocRuntimeType[T any]() unsafe.Pointer {
	t := reflect.TypeFor[T]()
	return (*[2]unsafe.Pointer)(unsafe.Pointer(&t))[1]
}

func TestGCMallocTypedPadding(t *testing.T) {
	typeInfo := gcMallocRuntimeType[gcMallocPointerRecord]()
	for iteration := 0; iteration < 256; iteration++ {
		record := (*gcMallocPointerRecord)(GCMalloc(
			typeInfo, uint64(unsafe.Sizeof(gcMallocPointerRecord{}))))
		if record == nil {
			t.Fatal("GCMalloc returned nil")
		}
		value := new(byte)
		*value = byte(iteration)
		_c2go_writePtr(&record.a, unsafe.Pointer(value))
		runtime.GC()
		if got := *(*byte)(record.a); got != byte(iteration) {
			t.Fatalf("managed pointer after GC = %d, want %d", got, byte(iteration))
		}
		runtime.KeepAlive(record)
	}
}
