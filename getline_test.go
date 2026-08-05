// SPDX-License-Identifier: AGPL-3.0-only

package libc

import (
	"os"
	"runtime"
	"strings"
	"testing"
	"unsafe"
)

func TestGetdelimAndGetline(t *testing.T) {
	prefix := strings.Repeat("abc", 100) + "|"
	const tail = "tail\n"
	path := t.TempDir() + "/getline.txt"
	if err := os.WriteFile(path, []byte(prefix+tail), 0o600); err != nil {
		t.Fatal(err)
	}

	pathBytes := append([]byte(path), 0)
	modeBytes := []byte{'r', 0}
	stream := Fopen(&pathBytes[0], &modeBytes[0])
	runtime.KeepAlive(pathBytes)
	runtime.KeepAlive(modeBytes)
	if stream == nil {
		t.Fatal("Fopen returned nil")
	}
	defer Fclose(stream)

	var line *byte
	var capacity uint64
	defer func() {
		if line != nil {
			Free(unsafe.Pointer(line))
		}
	}()

	if got := Getdelim(&line, &capacity, '|', stream); got != int64(len(prefix)) {
		t.Fatalf("Getdelim length = %d, want %d", got, len(prefix))
	}
	if got := cstr(line); got != prefix {
		t.Fatalf("Getdelim = %q, want %q", got, prefix)
	}
	if capacity <= uint64(len(prefix)) {
		t.Fatalf("Getdelim capacity = %d, want room for NUL", capacity)
	}
	first := line

	if got := Getline(&line, &capacity, stream); got != int64(len(tail)) {
		t.Fatalf("Getline length = %d, want %d", got, len(tail))
	}
	if got := cstr(line); got != tail {
		t.Fatalf("Getline = %q, want %q", got, tail)
	}
	if line != first {
		t.Fatal("Getline did not reuse the sufficiently large caller buffer")
	}
	if got := Getline(&line, &capacity, stream); got != -1 {
		t.Fatalf("Getline at EOF = %d, want -1", got)
	}
}
