// math_test.go — the math phase's growing test face. TestFloorProbe is the
// phase-validation probe (MIGRATION_PLAN: prove the libm.h shim mechanism on
// floor.c before scaling); the OLD math_*_test.go suites arrive with their
// waves. Oracle = Go's math package.
package libc

import (
	"math"
	"testing"
)

func TestFloorProbe(t *testing.T) {
	cases := []float64{3.7, -3.7, 0.5, -0.5, 0, 1, -1, 1e308, -1e308, 0.49999999999999994}
	for _, x := range cases {
		if got, want := Floor(x), math.Floor(x); got != want {
			t.Errorf("floor(%v) = %v, want %v", x, got, want)
		}
	}
	// sign of zero and non-finite pass-through
	if got := Floor(math.Copysign(0, -1)); math.Signbit(got) != true || got != 0 {
		t.Errorf("floor(-0) = %v, want -0", got)
	}
	if !math.IsNaN(Floor(math.NaN())) {
		t.Error("floor(NaN) must be NaN")
	}
	if got := Floor(math.Inf(-1)); !math.IsInf(got, -1) {
		t.Errorf("floor(-Inf) = %v", got)
	}
}
