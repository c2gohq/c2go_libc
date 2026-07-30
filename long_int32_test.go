//go:build windows && amd64

package libc

import "testing"

// testLong follows the target C long used by generated function signatures.
type testLong = int32

func testLongCanRepresent(v int64) bool {
	return v >= -1<<31 && v <= 1<<31-1
}

// Windows is LLP64, so scalbln's long exponent has no values outside int32.
func testScalblnWideLongClamping(t *testing.T) { t.Helper() }
