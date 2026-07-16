package libc

// Cross-checks the "hyper" cluster (sinh/cosh/tanh/asinh/acosh/atanh + f-variants,
// ported from musl-1.2.6) against Go's standard math package. Transcendentals are
// compared with a tight ULP bound (2 ULP for double, 4 ULP for float32 since the
// float reference is Go's double result rounded to float32). Specials (NaN, ±Inf,
// ±0) are checked exactly. These bindings do not exist until gen.sh runs, so this
// file is not expected to compile before then.

import (
	"math"
	"testing"
)

// ---- ULP helpers (hyper-prefixed to avoid clashes with other _test.go files) ----

func hyperOrd64(x float64) int64 {
	b := int64(math.Float64bits(x))
	if b < 0 {
		return math.MinInt64 - b
	}
	return b
}

func hyperUlp64(a, b float64) uint64 {
	d := hyperOrd64(a) - hyperOrd64(b)
	if d < 0 {
		d = -d
	}
	return uint64(d)
}

func hyperOrd32(x float32) int32 {
	b := int32(math.Float32bits(x))
	if b < 0 {
		return math.MinInt32 - b
	}
	return b
}

func hyperUlp32(a, b float32) uint32 {
	d := hyperOrd32(a) - hyperOrd32(b)
	if d < 0 {
		d = -d
	}
	return uint32(d)
}

func hyperClose64(t *testing.T, name string, x, got, want float64, tol uint64) {
	t.Helper()
	if math.IsNaN(want) {
		if !math.IsNaN(got) {
			t.Errorf("%s(%v) = %v, want NaN", name, x, got)
		}
		return
	}
	if math.IsInf(want, 0) {
		if got != want {
			t.Errorf("%s(%v) = %v, want %v", name, x, got, want)
		}
		return
	}
	if got == want { // also covers +0/-0 (0.0 == -0.0 in Go)
		return
	}
	if math.IsNaN(got) || math.IsInf(got, 0) {
		t.Errorf("%s(%v) = %v, want %v (finite)", name, x, got, want)
		return
	}
	if u := hyperUlp64(got, want); u > tol {
		t.Errorf("%s(%v) = %v, want %v (%d ulp > %d)", name, x, got, want, u, tol)
	}
}

func hyperClose32(t *testing.T, name string, x, got, want float32, tol uint32) {
	t.Helper()
	if math.IsNaN(float64(want)) {
		if !math.IsNaN(float64(got)) {
			t.Errorf("%s(%v) = %v, want NaN", name, x, got)
		}
		return
	}
	if math.IsInf(float64(want), 0) {
		if got != want {
			t.Errorf("%s(%v) = %v, want %v", name, x, got, want)
		}
		return
	}
	if got == want {
		return
	}
	if math.IsNaN(float64(got)) || math.IsInf(float64(got), 0) {
		t.Errorf("%s(%v) = %v, want %v (finite)", name, x, got, want)
		return
	}
	if u := hyperUlp32(got, want); u > tol {
		t.Errorf("%s(%v) = %v, want %v (%d ulp > %d)", name, x, got, want, u, tol)
	}
}

const hyperTolD = 2
const hyperTolF = 4

// Drivers: compare a C binding against a Go math reference over an input set.

func hyperRun64(t *testing.T, name string, f, ref func(float64) float64, xs []float64) {
	t.Helper()
	for _, x := range xs {
		hyperClose64(t, name, x, f(x), ref(x), hyperTolD)
	}
}

func hyperRun32(t *testing.T, name string, f func(float32) float32, ref func(float64) float64, xs []float32) {
	t.Helper()
	for _, x := range xs {
		hyperClose32(t, name, x, f(x), float32(ref(float64(x))), hyperTolF)
	}
}

// ---- input sets ----

// Full-domain inputs for sinh/cosh/tanh/asinh (double). Values near the overflow
// boundary of sinh/cosh (~710) are avoided: 700 is finite for both, 750 overflows
// for both.
var hyperGenD = []float64{
	0, math.Copysign(0, -1), 1, -1, 0.1, -0.1, 0.25, 0.5, -0.5, 0.9,
	1.5, 2, -2, 3, 5, -5, 10, 20, -20, 50, 100, 500, 700, -700, 750, -750,
	1e-10, 1e-300, 5e-324, // tiny + subnormal
	1e300,                 // huge (sinh/cosh -> Inf, asinh finite)
	math.Inf(1), math.Inf(-1), math.NaN(),
}

// acosh domain is x >= 1; sub-1 / negatives / NaN all yield NaN for both.
var hyperAcoshD = []float64{
	1, 1.0 + 1e-12, 1.0000001, 1.125, 1.5, 2, 5, 100, 1e8, 1e150, 1e300,
	0.5, 0, -1, -5, math.Inf(1), math.NaN(),
}

// atanh domain is |x| < 1; |x| >= 1 yields ±Inf (at 1) or NaN.
var hyperAtanhD = []float64{
	0, math.Copysign(0, -1), 0.1, -0.1, 0.25, 0.5, -0.5, 0.9, 0.99, 0.999,
	1e-10, 1e-300, 5e-324, 1, -1, 2, -2, math.Inf(1), math.Inf(-1), math.NaN(),
}

// float sets: sinhf/coshf overflow near |x|~88.7, so 80 is finite for both and
// 100 overflows for both.
var hyperGenF = []float32{
	0, float32(math.Copysign(0, -1)), 1, -1, 0.1, -0.1, 0.25, 0.5, -0.5, 0.9,
	1.5, 2, -2, 5, 10, 20, -20, 50, 80, -80, 100, -100,
	1e-10, 1e-20, float32(math.Inf(1)), float32(math.Inf(-1)), float32(math.NaN()),
}

var hyperAcoshF = []float32{
	1, 1.0000001, 1.125, 1.5, 2, 5, 100, 1e6, 1e20,
	0.5, 0, -1, float32(math.Inf(1)), float32(math.NaN()),
}

var hyperAtanhF = []float32{
	0, float32(math.Copysign(0, -1)), 0.1, -0.1, 0.25, 0.5, -0.5, 0.9, 0.99,
	1e-10, 1, -1, 2, float32(math.Inf(1)), float32(math.NaN()),
}

// ---- double tests ----

func TestSinh(t *testing.T)  { hyperRun64(t, "Sinh", Sinh, math.Sinh, hyperGenD) }
func TestCosh(t *testing.T)  { hyperRun64(t, "Cosh", Cosh, math.Cosh, hyperGenD) }
func TestTanh(t *testing.T)  { hyperRun64(t, "Tanh", Tanh, math.Tanh, hyperGenD) }
func TestAsinh(t *testing.T) { hyperRun64(t, "Asinh", Asinh, math.Asinh, hyperGenD) }
func TestAcosh(t *testing.T) { hyperRun64(t, "Acosh", Acosh, math.Acosh, hyperAcoshD) }
func TestAtanh(t *testing.T) { hyperRun64(t, "Atanh", Atanh, math.Atanh, hyperAtanhD) }

// ---- float tests ----

func TestSinhf(t *testing.T)  { hyperRun32(t, "Sinhf", Sinhf, math.Sinh, hyperGenF) }
func TestCoshf(t *testing.T)  { hyperRun32(t, "Coshf", Coshf, math.Cosh, hyperGenF) }
func TestTanhf(t *testing.T)  { hyperRun32(t, "Tanhf", Tanhf, math.Tanh, hyperGenF) }
func TestAsinhf(t *testing.T) { hyperRun32(t, "Asinhf", Asinhf, math.Asinh, hyperGenF) }
func TestAcoshf(t *testing.T) { hyperRun32(t, "Acoshf", Acoshf, math.Acosh, hyperAcoshF) }
func TestAtanhf(t *testing.T) { hyperRun32(t, "Atanhf", Atanhf, math.Atanh, hyperAtanhF) }
