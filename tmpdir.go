// tmpdir.go — the temp-directory helper behind tmpfile(3) on BOTH platforms
// (#651 unified the unix side; renamed from tmpfile_windows.go — the _windows
// FILENAME suffix is an implicit GOOS constraint that would exclude unix).
//
// os.TempDir is the single cross-world answer: $TMPDIR (falling back to /tmp)
// on unix — the prior unix tmpfile hard-coded /tmp, so a Go host's
// os.CreateTemp and C's tmpfile could land in different directories — and
// TMP/TEMP/USERPROFILE/GetTempPath on Windows, the CRT way. The C side
// appends its own "/tmpfile_XXXXXX" name, so the trailing separator is
// stripped here to avoid a doubled one.

package libc

import (
	"os"
	"unsafe"
	_ "unsafe" // for //go:linkname
)

// __c2go_tmpdir writes os.TempDir() (minus any trailing separator) plus a NUL
// into buf[:n], returning the number of bytes written (excluding the NUL), or -1
// if it would not fit.
//
//go:linkname __c2go_tmpdir
func __c2go_tmpdir(buf unsafe.Pointer, n int32) int32 {
	d := os.TempDir()
	for len(d) > 0 && (d[len(d)-1] == '\\' || d[len(d)-1] == '/') {
		d = d[:len(d)-1]
	}
	if int32(len(d))+1 > n { // room for the terminating NUL
		return -1
	}
	dst := unsafe.Slice((*byte)(buf), n)
	copy(dst, d)
	dst[len(d)] = 0
	return int32(len(d))
}
