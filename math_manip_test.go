package libc

// Cross-checks the bit-manipulation math ports (musl/src/math wave 1,
// verbatim musl fork) against Go's standard math package. Bindings are the C name
// Capitalized; int* out-params bind as *int32, const char* as *byte, C long as
// int64, C long double as float64. Most of these functions are exact (bit-ops
// / correct rounding) so results are compared bit-for-bit (Float64bits /
// Float32bits); fmaf is allowed 1 float32-ULP for the double-rounding halfway
// cases. Where C99 and Go math disagree (fdim/fmax/fmin on Inf-Inf and NaN,
// ilogb(NaN)), the C99-mandated result is asserted explicitly.

import (
	"math"
	"testing"
)

// ---- comparison helpers ----

func eq64(a, b float64) bool {
	if math.IsNaN(a) || math.IsNaN(b) {
		return math.IsNaN(a) && math.IsNaN(b)
	}
	return math.Float64bits(a) == math.Float64bits(b)
}

func eq32(a, b float32) bool {
	af, bf := float64(a), float64(b)
	if math.IsNaN(af) || math.IsNaN(bf) {
		return math.IsNaN(af) && math.IsNaN(bf)
	}
	return math.Float32bits(a) == math.Float32bits(b)
}

func ord32(x float32) int64 {
	b := math.Float32bits(x)
	if b&0x80000000 != 0 {
		return -int64(b & 0x7fffffff)
	}
	return int64(b)
}

func manipUlp32(a, b float32) int64 {
	d := ord32(a) - ord32(b)
	if d < 0 {
		d = -d
	}
	return d
}

var negZero = math.Copysign(0, -1)

var manipDvals = []float64{
	0, negZero, 1, -1, 0.5, -0.5, 2, -2, 3.14159265358979, -2.718281828,
	1e300, -1e300, 1e-300, -1e-300, 1234.5678, -9876.5432, 123456789.0,
	math.SmallestNonzeroFloat64, -math.SmallestNonzeroFloat64,
	math.MaxFloat64, -math.MaxFloat64, 0x1p-1050, // subnormal
}

var dedge = []float64{math.Inf(1), math.Inf(-1), math.NaN()}

var manipFvals = []float32{
	0, float32(negZero), 1, -1, 0.5, -0.5, 2, -2, 3.14159, -2.71828,
	1e30, -1e30, 1e-30, -1e-30, 1234.5, -9876.5, 65535.5,
	math.SmallestNonzeroFloat32, -math.SmallestNonzeroFloat32,
	math.MaxFloat32, -math.MaxFloat32, 0x1p-140, // subnormal float
}

var fedge = []float32{float32(math.Inf(1)), float32(math.Inf(-1)), float32(math.NaN())}

// ---- fabs ----

func TestFabsManip(t *testing.T) {
	for _, x := range append(append([]float64{}, manipDvals...), dedge...) {
		if got, want := Fabs(x), math.Abs(x); !eq64(got, want) {
			t.Errorf("Fabs(%v)=%v want %v", x, got, want)
		}
	}
	for _, x := range append(append([]float32{}, manipFvals...), fedge...) {
		if got, want := Fabsf(x), float32(math.Abs(float64(x))); !eq32(got, want) {
			t.Errorf("Fabsf(%v)=%v want %v", x, got, want)
		}
	}
}

// ---- copysign ----

func TestCopysignManip(t *testing.T) {
	xs := append(append([]float64{}, manipDvals...), dedge...)
	for _, x := range xs {
		for _, y := range xs {
			if got, want := Copysign(x, y), math.Copysign(x, y); !eq64(got, want) {
				t.Errorf("Copysign(%v,%v)=%v want %v", x, y, got, want)
			}
		}
	}
	fxs := append(append([]float32{}, manipFvals...), fedge...)
	for _, x := range fxs {
		for _, y := range fxs {
			want := float32(math.Copysign(float64(x), float64(y)))
			if got := Copysignf(x, y); !eq32(got, want) {
				t.Errorf("Copysignf(%v,%v)=%v want %v", x, y, got, want)
			}
		}
	}
}

// ---- frexp ----

func TestFrexpManip(t *testing.T) {
	for _, x := range manipDvals {
		var e int32
		frac := Frexp(x, &e)
		gf, ge := math.Frexp(x)
		if !eq64(frac, gf) || int(e) != ge {
			t.Errorf("Frexp(%v)=(%v,%d) want (%v,%d)", x, frac, e, gf, ge)
		}
	}
	// inf/nan: C leaves *e untouched and returns x; only check the value.
	for _, x := range dedge {
		var e int32 = 12345
		if frac := Frexp(x, &e); !eq64(frac, x) {
			t.Errorf("Frexp(%v)=%v want %v", x, frac, x)
		}
	}
	for _, x := range manipFvals {
		var e int32
		frac := Frexpf(x, &e)
		gf, ge := math.Frexp(float64(x))
		if !eq32(frac, float32(gf)) || int(e) != ge {
			t.Errorf("Frexpf(%v)=(%v,%d) want (%v,%d)", x, frac, e, float32(gf), ge)
		}
	}
	for _, x := range fedge {
		var e int32 = 12345
		if frac := Frexpf(x, &e); !eq32(frac, x) {
			t.Errorf("Frexpf(%v)=%v want %v", x, frac, x)
		}
	}
}

// ---- ldexp / scalbn / scalbln ----

func TestLdexpScalbnManip(t *testing.T) {
	ns := []int32{0, 1, -1, 10, -10, 52, 53, -52, -53, 60, -60, 1000, -1000, 2000, -2000, 1100, -1100}
	for _, x := range manipDvals {
		for _, n := range ns {
			want := math.Ldexp(x, int(n))
			if got := Ldexp(x, n); !eq64(got, want) {
				t.Errorf("Ldexp(%v,%d)=%v want %v", x, n, got, want)
			}
			if got := Scalbn(x, n); !eq64(got, want) {
				t.Errorf("Scalbn(%v,%d)=%v want %v", x, n, got, want)
			}
			if got := Scalbln(x, int64(n)); !eq64(got, want) {
				t.Errorf("Scalbln(%v,%d)=%v want %v", x, n, got, want)
			}
		}
	}
	// long-clamping paths of scalbln
	if got, want := Scalbln(1, 1<<40), math.Ldexp(1, math.MaxInt32); !eq64(got, want) {
		t.Errorf("Scalbln(1,1<<40)=%v want %v", got, want)
	}
	if got, want := Scalbln(1, -(1 << 40)), math.Ldexp(1, math.MinInt32); !eq64(got, want) {
		t.Errorf("Scalbln(1,-(1<<40))=%v want %v", got, want)
	}

	fns := []int32{0, 1, -1, 10, -10, 23, 24, -23, -24, 120, -120, 130, -130, 300, -300, 500, -500}
	for _, x := range manipFvals {
		for _, n := range fns {
			want := float32(math.Ldexp(float64(x), int(n)))
			if got := Ldexpf(x, n); !eq32(got, want) {
				t.Errorf("Ldexpf(%v,%d)=%v want %v", x, n, got, want)
			}
			if got := Scalbnf(x, n); !eq32(got, want) {
				t.Errorf("Scalbnf(%v,%d)=%v want %v", x, n, got, want)
			}
			if got := Scalblnf(x, int64(n)); !eq32(got, want) {
				t.Errorf("Scalblnf(%v,%d)=%v want %v", x, n, got, want)
			}
		}
	}
	// inf/nan pass-through
	for _, x := range dedge {
		if got, want := Scalbn(x, 3), math.Ldexp(x, 3); !eq64(got, want) {
			t.Errorf("Scalbn(%v,3)=%v want %v", x, got, want)
		}
	}
}

// ---- ilogb ----

func TestIlogbManip(t *testing.T) {
	for _, x := range manipDvals {
		if x == 0 { // ilogb(0) special-cased below
			continue
		}
		if got, want := int(Ilogb(x)), math.Ilogb(x); got != want {
			t.Errorf("Ilogb(%v)=%d want %d", x, got, want)
		}
	}
	// C99: ilogb(0)=FP_ILOGB0=INT_MIN, ilogb(Inf)=INT_MAX, ilogb(NaN)=FP_ILOGBNAN=INT_MIN
	if got := Ilogb(0); got != math.MinInt32 {
		t.Errorf("Ilogb(0)=%d want %d", got, math.MinInt32)
	}
	if got := Ilogb(negZero); got != math.MinInt32 {
		t.Errorf("Ilogb(-0)=%d want %d", got, math.MinInt32)
	}
	if got := Ilogb(math.Inf(1)); got != math.MaxInt32 {
		t.Errorf("Ilogb(+Inf)=%d want %d", got, math.MaxInt32)
	}
	if got := Ilogb(math.Inf(-1)); got != math.MaxInt32 {
		t.Errorf("Ilogb(-Inf)=%d want %d", got, math.MaxInt32)
	}
	if got := Ilogb(math.NaN()); got != math.MinInt32 {
		t.Errorf("Ilogb(NaN)=%d want %d", got, math.MinInt32)
	}

	for _, x := range manipFvals {
		if x == 0 {
			continue
		}
		if got, want := int(Ilogbf(x)), math.Ilogb(float64(x)); got != want {
			t.Errorf("Ilogbf(%v)=%d want %d", x, got, want)
		}
	}
	if got := Ilogbf(0); got != math.MinInt32 {
		t.Errorf("Ilogbf(0)=%d want %d", got, math.MinInt32)
	}
	if got := Ilogbf(float32(math.Inf(1))); got != math.MaxInt32 {
		t.Errorf("Ilogbf(+Inf)=%d want %d", got, math.MaxInt32)
	}
	if got := Ilogbf(float32(math.NaN())); got != math.MinInt32 {
		t.Errorf("Ilogbf(NaN)=%d want %d", got, math.MinInt32)
	}
}

// ---- logb ----

func TestLogbManip(t *testing.T) {
	all := append(append([]float64{}, manipDvals...), dedge...)
	for _, x := range all {
		if got, want := Logb(x), math.Logb(x); !eq64(got, want) {
			t.Errorf("Logb(%v)=%v want %v", x, got, want)
		}
	}
	if got := Logb(0); got != math.Inf(-1) {
		t.Errorf("Logb(0)=%v want -Inf", got)
	}
	fall := append(append([]float32{}, manipFvals...), fedge...)
	for _, x := range fall {
		want := float32(math.Logb(float64(x)))
		if got := Logbf(x); !eq32(got, want) {
			t.Errorf("Logbf(%v)=%v want %v", x, got, want)
		}
	}
}

// ---- nextafter / nexttoward ----

func TestNextafterManip(t *testing.T) {
	pairs := [][2]float64{
		{1, 2}, {1, 0}, {-1, -2}, {-1, 0}, {2, 1}, {0.5, 1}, {1, 1},
		{1e300, math.Inf(1)}, {math.MaxFloat64, math.Inf(1)},
		{0, 1}, {0, -1}, {5e-324, 0}, {5e-324, 1},
		{123.456, 123.457}, {-9.99, -10.0},
	}
	for _, p := range pairs {
		want := math.Nextafter(p[0], p[1])
		if got := Nextafter(p[0], p[1]); !eq64(got, want) {
			t.Errorf("Nextafter(%v,%v)=%v want %v", p[0], p[1], got, want)
		}
		// nexttoward(double, long double==double) == nextafter
		if got := Nexttoward(p[0], p[1]); !eq64(got, want) {
			t.Errorf("Nexttoward(%v,%v)=%v want %v", p[0], p[1], got, want)
		}
	}
	// C99: nextafter(+0,-0) = -0 (Go math.Nextafter returns +0 for x==y).
	if got := Nextafter(0, negZero); math.Float64bits(got) != math.Float64bits(negZero) {
		t.Errorf("Nextafter(+0,-0)=%v want -0", got)
	}
	// NaN propagation
	if got := Nextafter(math.NaN(), 1); !math.IsNaN(got) {
		t.Errorf("Nextafter(NaN,1)=%v want NaN", got)
	}

	fpairs := [][2]float32{
		{1, 2}, {1, 0}, {-1, -2}, {2, 1}, {0.5, 1}, {1, 1},
		{1e30, float32(math.Inf(1))}, {math.MaxFloat32, float32(math.Inf(1))},
		{0, 1}, {math.SmallestNonzeroFloat32, 0}, {123.4, 123.5},
	}
	for _, p := range fpairs {
		want := math.Nextafter32(p[0], p[1])
		if got := Nextafterf(p[0], p[1]); !eq32(got, want) {
			t.Errorf("Nextafterf(%v,%v)=%v want %v", p[0], p[1], got, want)
		}
	}
	if got := Nextafterf(float32(math.NaN()), 1); !math.IsNaN(float64(got)) {
		t.Errorf("Nextafterf(NaN,1)=%v want NaN", got)
	}
}

func TestNexttowardfManip(t *testing.T) {
	// nexttowardf(x, y): step x one float toward the (double) target y; only the
	// direction of y relative to x matters.
	cases := []struct {
		x float32
		y float64
	}{
		{1, 2}, {1, 0.5}, {1, 1.0000000001}, {1, 0.9999999999},
		{-1, 0}, {-1, -2}, {123.4, 200}, {123.4, 0}, {0, 1}, {0, -1},
	}
	for _, c := range cases {
		var want float32
		switch {
		case float64(c.x) == c.y:
			want = c.x
		case float64(c.x) < c.y:
			want = math.Nextafter32(c.x, float32(math.Inf(1)))
		default:
			want = math.Nextafter32(c.x, float32(math.Inf(-1)))
		}
		if got := Nexttowardf(c.x, c.y); !eq32(got, want) {
			t.Errorf("Nexttowardf(%v,%v)=%v want %v", c.x, c.y, got, want)
		}
	}
	if got := Nexttowardf(float32(math.NaN()), 1); !math.IsNaN(float64(got)) {
		t.Errorf("Nexttowardf(NaN,1)=%v want NaN", got)
	}
}

// ---- nan ----

func TestNanManip(t *testing.T) {
	z := byte(0)
	if got := Nan(&z); !math.IsNaN(got) {
		t.Errorf("Nan()=%v want NaN", got)
	}
	if got := Nanf(&z); !math.IsNaN(float64(got)) {
		t.Errorf("Nanf()=%v want NaN", got)
	}
}

// ---- fdim ----

func TestFdimManip(t *testing.T) {
	// finite cases match Go math.Dim
	fin := []float64{0, 1, -1, 2, 3.5, -3.5, 1e300, -1e300, 100, 99}
	for _, x := range fin {
		for _, y := range fin {
			want := math.Dim(x, y)
			if got := Fdim(x, y); !eq64(got, want) {
				t.Errorf("Fdim(%v,%v)=%v want %v", x, y, got, want)
			}
		}
	}
	// C99 edges: fdim(Inf,Inf)=0 (Go math.Dim gives NaN); NaN propagates.
	if got := Fdim(math.Inf(1), math.Inf(1)); got != 0 {
		t.Errorf("Fdim(+Inf,+Inf)=%v want 0", got)
	}
	if got := Fdim(math.Inf(1), 1); got != math.Inf(1) {
		t.Errorf("Fdim(+Inf,1)=%v want +Inf", got)
	}
	if got := Fdim(1, math.Inf(1)); got != 0 {
		t.Errorf("Fdim(1,+Inf)=%v want 0", got)
	}
	if got := Fdim(math.NaN(), 1); !math.IsNaN(got) {
		t.Errorf("Fdim(NaN,1)=%v want NaN", got)
	}
	if got := Fdim(1, math.NaN()); !math.IsNaN(got) {
		t.Errorf("Fdim(1,NaN)=%v want NaN", got)
	}

	ffin := []float32{0, 1, -1, 2, 3.5, -3.5, 1e30, -1e30, 100, 99}
	for _, x := range ffin {
		for _, y := range ffin {
			want := float32(math.Dim(float64(x), float64(y)))
			if got := Fdimf(x, y); !eq32(got, want) {
				t.Errorf("Fdimf(%v,%v)=%v want %v", x, y, got, want)
			}
		}
	}
	if got := Fdimf(float32(math.Inf(1)), float32(math.Inf(1))); got != 0 {
		t.Errorf("Fdimf(+Inf,+Inf)=%v want 0", got)
	}
	if got := Fdimf(float32(math.NaN()), 1); !math.IsNaN(float64(got)) {
		t.Errorf("Fdimf(NaN,1)=%v want NaN", got)
	}
}

// ---- fmax / fmin ----

func TestFmaxFminManip(t *testing.T) {
	fin := []float64{0, negZero, 1, -1, 2, -2, 3.5, -3.5, 1e300, -1e300}
	for _, x := range fin {
		for _, y := range fin {
			// signed-zero: C99 F.9.9.2 — fmax favors +0, fmin favors -0.
			var wmax, wmin float64
			if signbit64(x) != signbit64(y) {
				if signbit64(x) {
					wmax, wmin = y, x
				} else {
					wmax, wmin = x, y
				}
			} else if x < y {
				wmax, wmin = y, x
			} else {
				wmax, wmin = x, y
			}
			if got := Fmax(x, y); !eq64(got, wmax) {
				t.Errorf("Fmax(%v,%v)=%v want %v", x, y, got, wmax)
			}
			if got := Fmin(x, y); !eq64(got, wmin) {
				t.Errorf("Fmin(%v,%v)=%v want %v", x, y, got, wmin)
			}
		}
	}
	// C99: fmax/fmin return the non-NaN operand.
	if got := Fmax(math.NaN(), 3); got != 3 {
		t.Errorf("Fmax(NaN,3)=%v want 3", got)
	}
	if got := Fmax(3, math.NaN()); got != 3 {
		t.Errorf("Fmax(3,NaN)=%v want 3", got)
	}
	if got := Fmin(math.NaN(), 3); got != 3 {
		t.Errorf("Fmin(NaN,3)=%v want 3", got)
	}
	if got := Fmin(3, math.NaN()); got != 3 {
		t.Errorf("Fmin(3,NaN)=%v want 3", got)
	}
	if got := Fmax(math.NaN(), math.NaN()); !math.IsNaN(got) {
		t.Errorf("Fmax(NaN,NaN)=%v want NaN", got)
	}

	ffin := []float32{0, float32(negZero), 1, -1, 2, -2, 3.5, -3.5, 1e30, -1e30}
	for _, x := range ffin {
		for _, y := range ffin {
			var wmax, wmin float32
			if signbit32(x) != signbit32(y) {
				if signbit32(x) {
					wmax, wmin = y, x
				} else {
					wmax, wmin = x, y
				}
			} else if x < y {
				wmax, wmin = y, x
			} else {
				wmax, wmin = x, y
			}
			if got := Fmaxf(x, y); !eq32(got, wmax) {
				t.Errorf("Fmaxf(%v,%v)=%v want %v", x, y, got, wmax)
			}
			if got := Fminf(x, y); !eq32(got, wmin) {
				t.Errorf("Fminf(%v,%v)=%v want %v", x, y, got, wmin)
			}
		}
	}
	if got := Fmaxf(float32(math.NaN()), 3); got != 3 {
		t.Errorf("Fmaxf(NaN,3)=%v want 3", got)
	}
	if got := Fminf(3, float32(math.NaN())); got != 3 {
		t.Errorf("Fminf(3,NaN)=%v want 3", got)
	}
}

func signbit64(x float64) bool { return math.Float64bits(x)&(1<<63) != 0 }
func signbit32(x float32) bool { return math.Float32bits(x)&(1<<31) != 0 }

// ---- fma ----

func TestFmaManip(t *testing.T) {
	cases := [][3]float64{
		{2, 3, 4}, {1.5, 2.5, -0.25}, {-2, 3, 5}, {1e300, 1e300, -1e300},
		{0x1p-540, 0x1p-540, 0x1p-1060}, {math.Pi, math.E, -1}, {1, 1, 1},
		{123456.789, 0.000123, 42.5}, {-7.25, 8.5, 100.125},
		{0x1.fffffffffffffp1023, 2, 0}, // overflow
		{0x1p-1074, 0x1p-1074, 0x1p-1074}, // tiny/subnormal
	}
	for _, c := range cases {
		want := math.FMA(c[0], c[1], c[2])
		if got := Fma(c[0], c[1], c[2]); !eq64(got, want) {
			t.Errorf("Fma(%v,%v,%v)=%v want %v (bits %x vs %x)",
				c[0], c[1], c[2], got, want, math.Float64bits(got), math.Float64bits(want))
		}
	}
	// special values
	if got := Fma(math.Inf(1), 2, 3); got != math.Inf(1) {
		t.Errorf("Fma(+Inf,2,3)=%v want +Inf", got)
	}
	if got := Fma(0, math.Inf(1), 1); !math.IsNaN(got) {
		t.Errorf("Fma(0,+Inf,1)=%v want NaN", got)
	}
	if got := Fma(math.NaN(), 1, 1); !math.IsNaN(got) {
		t.Errorf("Fma(NaN,1,1)=%v want NaN", got)
	}

	fcases := [][3]float32{
		{2, 3, 4}, {1.5, 2.5, -0.25}, {-2, 3, 5}, {1e30, 1e10, -1e38},
		{math.Pi, math.E, -1}, {0x1p-100, 0x1p-100, 0x1p-149},
		{123.456, 0.00789, 42.5}, {-7.25, 8.5, 100.125},
	}
	for _, c := range fcases {
		want := float32(math.FMA(float64(c[0]), float64(c[1]), float64(c[2])))
		got := Fmaf(c[0], c[1], c[2])
		if !eq32(got, want) && manipUlp32(got, want) > 1 {
			t.Errorf("Fmaf(%v,%v,%v)=%v want ~%v (%d ulp)",
				c[0], c[1], c[2], got, want, manipUlp32(got, want))
		}
	}
	if got := Fmaf(float32(math.Inf(1)), 2, 3); got != float32(math.Inf(1)) {
		t.Errorf("Fmaf(+Inf,2,3)=%v want +Inf", got)
	}
	if got := Fmaf(float32(math.NaN()), 1, 1); !math.IsNaN(float64(got)) {
		t.Errorf("Fmaf(NaN,1,1)=%v want NaN", got)
	}
}
