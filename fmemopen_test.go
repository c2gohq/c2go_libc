// SPDX-License-Identifier: AGPL-3.0-only

package libc

import (
	"runtime"
	"testing"
	"unsafe"
)

func TestFmemopenRoundTrip(t *testing.T) {
	storage := make([]byte, 64)
	mode := []byte{'w', '+', 0}
	stream := Fmemopen(unsafe.Pointer(&storage[0]), uint64(len(storage)), &mode[0])
	if stream == nil {
		t.Fatal("Fmemopen returned nil")
	}

	text := append([]byte("memory-stream"), 0)
	if got := Fputs(&text[0], stream); got < 0 {
		t.Fatalf("Fputs returned %d", got)
	}
	if got := Fseek(stream, 0, 0); got != 0 {
		t.Fatalf("Fseek returned %d", got)
	}
	out := make([]byte, len(text)-1)
	if got := Fread(unsafe.Pointer(&out[0]), 1, uint64(len(out)), stream); got != uint64(len(out)) {
		t.Fatalf("Fread returned %d, want %d", got, len(out))
	}
	if got := string(out); got != "memory-stream" {
		t.Fatalf("Fmemopen round trip = %q", got)
	}
	if got := Fclose(stream); got != 0 {
		t.Fatalf("Fclose returned %d", got)
	}
	runtime.KeepAlive(storage)
	runtime.KeepAlive(mode)
	runtime.KeepAlive(text)
}
