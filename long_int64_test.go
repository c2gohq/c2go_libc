//go:build darwin || linux

package libc

import (
	"math"
	"testing"
)

// testLong follows the target C long used by generated function signatures.
type testLong = int64

func testLongCanRepresent(int64) bool { return true }

func testScalblnWideLongClamping(t *testing.T) {
	t.Helper()
	if got, want := Scalbln(1, 1<<40), math.Ldexp(1, math.MaxInt32); !eq64(got, want) {
		t.Errorf("Scalbln(1,1<<40)=%v want %v", got, want)
	}
	if got, want := Scalbln(1, -(1<<40)), math.Ldexp(1, math.MinInt32); !eq64(got, want) {
		t.Errorf("Scalbln(1,-(1<<40))=%v want %v", got, want)
	}
}
