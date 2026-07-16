package libc

// Cross-checks the "trig" cluster (sin/cos/tan/sincos/asin/acos/atan/atan2 +
// f-variants, ported from musl-1.2.6 src/math) against a correctly-rounded
// high-precision oracle (mpmath, see math_oracle_test.go) rather than Go's
// math package: Go's transcendentals are only ~1-ULP accurate and are wrong by
// up to hundreds of ULP at several of these inputs, whereas the port is
// correctly rounded there, so Go is a bad reference. The finite transcendental
// results are compared against the oracle with a tight ULP bound (2 ULP for
// double, 4 ULP for float32 since the float reference is the oracle's double
// result rounded to float32). Specials (NaN, ±Inf, ±0, out-of-domain) are not
// in the oracle and fall back to Go's math, which is exact for them; they are
// checked exactly as before. These bindings do not exist until gen.sh runs, so
// this file is not expected to compile before then.

import (
	"math"
	"testing"
)

// ---- ULP helpers (trig-prefixed to avoid clashes with other _test.go files) ----

func trigOrd64(x float64) int64 {
	b := int64(math.Float64bits(x))
	if b < 0 {
		return math.MinInt64 - b
	}
	return b
}

func trigUlp64(a, b float64) uint64 {
	d := trigOrd64(a) - trigOrd64(b)
	if d < 0 {
		d = -d
	}
	return uint64(d)
}

func trigOrd32(x float32) int32 {
	b := int32(math.Float32bits(x))
	if b < 0 {
		return math.MinInt32 - b
	}
	return b
}

func trigUlp32(a, b float32) uint32 {
	d := trigOrd32(a) - trigOrd32(b)
	if d < 0 {
		d = -d
	}
	return uint32(d)
}

func trigClose64(t *testing.T, name string, x, got, want float64, tol uint64) {
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
	if u := trigUlp64(got, want); u > tol {
		t.Errorf("%s(%v) = %v, want %v (%d ulp > %d)", name, x, got, want, u, tol)
	}
}

func trigClose32(t *testing.T, name string, x, got, want float32, tol uint32) {
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
	if u := trigUlp32(got, want); u > tol {
		t.Errorf("%s(%v) = %v, want %v (%d ulp > %d)", name, x, got, want, u, tol)
	}
}

const trigTolD = 2
const trigTolF = 4

func trigRun64(t *testing.T, name string, f, ref func(float64) float64, xs []float64) {
	t.Helper()
	for _, x := range xs {
		trigClose64(t, name, x, f(x), ref(x), trigTolD)
	}
}

func trigRun32(t *testing.T, name string, f func(float32) float32, ref func(float64) float64, xs []float32) {
	t.Helper()
	for _, x := range xs {
		trigClose32(t, name, x, f(x), float32(ref(float64(x))), trigTolF)
	}
}

// ---- input sets ----

// Angles for sin/cos/tan/sincos. Cover: ±0, tiny/subnormal, sub-pi/4, near the
// pi/2 boundaries, a spread through several periods, and large magnitudes that
// exercise the Payne-Hanek reduction (__rem_pio2_large).
var trigAngD = []float64{
	0, math.Copysign(0, -1),
	1e-300, 5e-324, 1e-20, 1e-10, // tiny + subnormal
	1e-8, 0.3, -0.3, math.Pi / 6, math.Pi / 4, -math.Pi / 4,
	0.7, 1.0, -1.0, 1.5, math.Pi / 2, -math.Pi / 2,
	2.0, 3.0, math.Pi, -math.Pi, 3.5, 4.0, 3 * math.Pi / 2,
	5.0, 2 * math.Pi, 6.5, 10.0, -10.0, 100.0, 1000.5,
	12345.6789, 1e6, 1e9, 1e15, 1e18, 1e300, -1e300,
	math.Inf(1), math.Inf(-1), math.NaN(),
}

var trigAngF = []float32{
	0, float32(math.Copysign(0, -1)),
	1e-20, 1e-8, 0.3, -0.3, float32(math.Pi / 4), -float32(math.Pi / 4),
	0.7, 1.0, -1.0, 1.5, float32(math.Pi / 2), -float32(math.Pi / 2),
	2.0, 3.0, float32(math.Pi), 3.5, 4.0, 5.0, float32(2 * math.Pi),
	6.5, 10.0, -10.0, 100.0, 1000.5, 12345.678, 1e6, 1e12, 1e20,
	float32(math.Inf(1)), float32(math.Inf(-1)), float32(math.NaN()),
}

// asin/acos domain is [-1,1]; outside yields NaN for both.
var trigUnitD = []float64{
	0, math.Copysign(0, -1), 1e-300, 5e-324, 1e-20, 1e-10,
	0.1, -0.1, 0.25, 0.4, 0.5, -0.5, 0.6, 0.7, 0.9, 0.95, 0.975, 0.99, 0.9999,
	1.0, -1.0, 1.0000001, 2.0, -2.0, 1e300, math.Inf(1), math.Inf(-1), math.NaN(),
}

var trigUnitF = []float32{
	0, float32(math.Copysign(0, -1)), 1e-20, 1e-10,
	0.1, -0.1, 0.25, 0.4, 0.5, -0.5, 0.6, 0.7, 0.9, 0.95, 0.99, 0.9999,
	1.0, -1.0, 1.0000001, 2.0, -2.0, float32(math.Inf(1)), float32(math.NaN()),
}

// atan domain is all reals.
var trigAtanD = []float64{
	0, math.Copysign(0, -1), 1e-300, 5e-324, 1e-20, 1e-10,
	0.1, -0.1, 0.4, 0.4375, 0.5, 0.6875, 0.7, 1.0, -1.0, 1.1875, 1.5,
	2.0, 2.4375, 3.0, 5.0, 10.0, -10.0, 100.0, 1e6, 1e15, 7.4e19, 1e300, -1e300,
	math.Inf(1), math.Inf(-1), math.NaN(),
}

var trigAtanF = []float32{
	0, float32(math.Copysign(0, -1)), 1e-20, 1e-10,
	0.1, -0.1, 0.4, 0.4375, 0.5, 0.6875, 0.7, 1.0, -1.0, 1.1875, 1.5,
	2.0, 2.4375, 3.0, 5.0, 10.0, -10.0, 100.0, 1e6, 7e7, 1e20,
	float32(math.Inf(1)), float32(math.Inf(-1)), float32(math.NaN()),
}

// ---- sin / cos / tan (double) ----

func TestSin(t *testing.T) { trigRun64(t, "Sin", Sin, sinRef, trigAngD) }
func TestCos(t *testing.T) { trigRun64(t, "Cos", Cos, cosRef, trigAngD) }
func TestTan(t *testing.T) { trigRun64(t, "Tan", Tan, tanRef, trigAngD) }

func TestSinf(t *testing.T) { trigRun32(t, "Sinf", Sinf, sinRef, trigAngF) }
func TestCosf(t *testing.T) { trigRun32(t, "Cosf", Cosf, cosRef, trigAngF) }
func TestTanf(t *testing.T) { trigRun32(t, "Tanf", Tanf, tanRef, trigAngF) }

// ---- sincos / sincosf ----

func TestSincos(t *testing.T) {
	for _, x := range trigAngD {
		var s, c float64
		Sincos(x, &s, &c)
		trigClose64(t, "Sincos.sin", x, s, sinRef(x), trigTolD)
		trigClose64(t, "Sincos.cos", x, c, cosRef(x), trigTolD)
	}
}

func TestSincosf(t *testing.T) {
	for _, x := range trigAngF {
		var s, c float32
		Sincosf(x, &s, &c)
		trigClose32(t, "Sincosf.sin", x, s, float32(sinRef(float64(x))), trigTolF)
		trigClose32(t, "Sincosf.cos", x, c, float32(cosRef(float64(x))), trigTolF)
	}
}

// ---- asin / acos ----

func TestAsin(t *testing.T) { trigRun64(t, "Asin", Asin, asinRef, trigUnitD) }
func TestAcos(t *testing.T) { trigRun64(t, "Acos", Acos, acosRef, trigUnitD) }

func TestAsinf(t *testing.T) { trigRun32(t, "Asinf", Asinf, asinRef, trigUnitF) }
func TestAcosf(t *testing.T) { trigRun32(t, "Acosf", Acosf, acosRef, trigUnitF) }

// ---- atan ----

func TestAtan(t *testing.T)  { trigRun64(t, "Atan", Atan, atanRef, trigAtanD) }
func TestAtanf(t *testing.T) { trigRun32(t, "Atanf", Atanf, atanRef, trigAtanF) }

// ---- atan2 / atan2f ----

// Pairs (y, x) covering the sign quadrants plus the ±0/±Inf special cases the
// switch(m) tables handle.
var trigAtan2Pairs = [][2]float64{
	{0, 0}, {0, math.Copysign(0, -1)}, {math.Copysign(0, -1), 0},
	{math.Copysign(0, -1), math.Copysign(0, -1)},
	{1, 1}, {1, -1}, {-1, 1}, {-1, -1},
	{0, 1}, {0, -1}, {1, 0}, {-1, 0},
	{3, 4}, {-3, 4}, {3, -4}, {-3, -4},
	{1e300, 1e-300}, {1e-300, 1e300}, {1e-300, -1e300},
	{2.5, 0.5}, {-2.5, 0.5}, {0.5, 2.5}, {0.5, -2.5},
	{math.Inf(1), math.Inf(1)}, {math.Inf(1), math.Inf(-1)},
	{math.Inf(-1), math.Inf(1)}, {math.Inf(-1), math.Inf(-1)},
	{1, math.Inf(1)}, {1, math.Inf(-1)}, {math.Inf(1), 1}, {math.Inf(-1), 1},
	{math.NaN(), 1}, {1, math.NaN()},
}

func TestAtan2(t *testing.T) {
	for _, p := range trigAtan2Pairs {
		y, x := p[0], p[1]
		got := Atan2(y, x)
		want := atan2Ref(y, x)
		trigClose64(t, "Atan2", y, got, want, trigTolD)
	}
}

func TestAtan2f(t *testing.T) {
	for _, p := range trigAtan2Pairs {
		y, x := float32(p[0]), float32(p[1])
		got := Atan2f(y, x)
		want := float32(atan2Ref(float64(y), float64(x)))
		trigClose32(t, "Atan2f", y, got, want, trigTolF)
	}
}
