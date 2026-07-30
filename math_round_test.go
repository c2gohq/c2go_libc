package libc

// Cross-check the rounding / integer-part ports (musl/src/math, wave 1) against
// Go's standard math package. Every function here is algebraic/rounding, so the
// expected relation is BIT-EXACT (via Float{32,64}bits, so signed zero is
// distinguished; NaN inputs only require both sides be NaN).
//
// Rounding-mode mapping:
//   round / roundf   -> math.Round        (half away from zero)
//   rint / rintf     -> math.RoundToEven  (half to even = default FP rounding)
//   nearbyint(f)     -> math.RoundToEven  (same as rint here)
//   lrint/llrint     -> int64(RoundToEven)
//   lround/llround   -> int64(Round)
//
// NOTE: the Go bindings (Floor, Ceil, ..., Modf) do not exist until gen.sh has
// run, so this file will not compile standalone yet — that is expected.

import (
	"math"
	"testing"
)

func f64eq(a, b float64) bool {
	if math.IsNaN(a) || math.IsNaN(b) {
		return math.IsNaN(a) && math.IsNaN(b)
	}
	return math.Float64bits(a) == math.Float64bits(b)
}

func f32eq(a, b float32) bool {
	if math.IsNaN(float64(a)) || math.IsNaN(float64(b)) {
		return math.IsNaN(float64(a)) && math.IsNaN(float64(b))
	}
	return math.Float32bits(a) == math.Float32bits(b)
}

// A spread of doubles covering the required edge cases.
var roundVals = []float64{
	0.0, math.Copysign(0, -1), // +0, -0
	math.Inf(1), math.Inf(-1), math.NaN(),
	1, -1, 0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 3.5, -3.5,
	2.3, -2.3, 2.7, -2.7, 0.49999999999999994, -0.49999999999999994,
	0.9999999999999999, -0.9999999999999999,
	4503599627370495.5,  // 2^52 - 0.5
	4503599627370496,    // 2^52 (first double with no fractional bits)
	9007199254740992,    // 2^53
	8388608, -8388608,   // 2^23
	123456.789, -123456.789,
	1e300, -1e300, 1e-300, -1e-300,
	5e-324, -5e-324,     // smallest subnormal
	2.2250738585072014e-308, // smallest normal
	math.Pi, -math.Pi, math.E,
}

// Finite, in-range-for-int64 subset (avoids Inf/NaN -> long UB divergence).
var intSafeVals = []float64{
	0.0, math.Copysign(0, -1),
	1, -1, 0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 3.5, -3.5,
	2.3, -2.3, 2.7, -2.7,
	123456.789, -123456.789,
	1e15, -1e15, 4503599627370496, -4503599627370496,
	42, -42,
}

func TestRoundFloor(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Floor(x), math.Floor(x); !f64eq(got, want) {
			t.Errorf("Floor(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Floorf(float32(x)), float32(math.Floor(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Floorf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundCeil(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Ceil(x), math.Ceil(x); !f64eq(got, want) {
			t.Errorf("Ceil(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Ceilf(float32(x)), float32(math.Ceil(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Ceilf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundTrunc(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Trunc(x), math.Trunc(x); !f64eq(got, want) {
			t.Errorf("Trunc(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Truncf(float32(x)), float32(math.Trunc(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Truncf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundRound(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Round(x), math.Round(x); !f64eq(got, want) {
			t.Errorf("Round(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Roundf(float32(x)), float32(math.Round(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Roundf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundRint(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Rint(x), math.RoundToEven(x); !f64eq(got, want) {
			t.Errorf("Rint(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Rintf(float32(x)), float32(math.RoundToEven(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Rintf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundNearbyint(t *testing.T) {
	for _, x := range roundVals {
		if got, want := Nearbyint(x), math.RoundToEven(x); !f64eq(got, want) {
			t.Errorf("Nearbyint(%v) = %v (%#x), want %v (%#x)", x, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		if got, want := Nearbyintf(float32(x)), float32(math.RoundToEven(float64(float32(x)))); !f32eq(got, want) {
			t.Errorf("Nearbyintf(%v) = %v, want %v", float32(x), got, want)
		}
	}
}

func TestRoundLrint(t *testing.T) {
	for _, x := range intSafeVals {
		want := int64(math.RoundToEven(x))
		if testLongCanRepresent(want) {
			if got := Lrint(x); int64(got) != want {
				t.Errorf("Lrint(%v) = %d, want %d", x, got, want)
			}
		}
		if got := Llrint(x); got != want {
			t.Errorf("Llrint(%v) = %d, want %d", x, got, want)
		}
		fx := float32(x)
		wantf := int64(math.RoundToEven(float64(fx)))
		if testLongCanRepresent(wantf) {
			if got := Lrintf(fx); int64(got) != wantf {
				t.Errorf("Lrintf(%v) = %d, want %d", fx, got, wantf)
			}
		}
		if got := Llrintf(fx); got != wantf {
			t.Errorf("Llrintf(%v) = %d, want %d", fx, got, wantf)
		}
	}
}

func TestRoundLround(t *testing.T) {
	for _, x := range intSafeVals {
		want := int64(math.Round(x))
		if testLongCanRepresent(want) {
			if got := Lround(x); int64(got) != want {
				t.Errorf("Lround(%v) = %d, want %d", x, got, want)
			}
		}
		if got := Llround(x); got != want {
			t.Errorf("Llround(%v) = %d, want %d", x, got, want)
		}
		fx := float32(x)
		wantf := int64(math.Round(float64(fx)))
		if testLongCanRepresent(wantf) {
			if got := Lroundf(fx); int64(got) != wantf {
				t.Errorf("Lroundf(%v) = %d, want %d", fx, got, wantf)
			}
		}
		if got := Llroundf(fx); got != wantf {
			t.Errorf("Llroundf(%v) = %d, want %d", fx, got, wantf)
		}
	}
}

func TestRoundModf(t *testing.T) {
	for _, x := range roundVals {
		if math.IsInf(x, 0) || math.IsNaN(x) {
			continue // POSIX modf(±Inf) = ±0 differs from Go's Modf(±Inf) = NaN
		}
		goInt, goFrac := math.Modf(x)
		var ip float64
		frac := Modf(x, &ip)
		if !f64eq(ip, goInt) {
			t.Errorf("Modf(%v) iptr = %v (%#x), want %v (%#x)", x, ip, math.Float64bits(ip), goInt, math.Float64bits(goInt))
		}
		if !f64eq(frac, goFrac) {
			t.Errorf("Modf(%v) frac = %v (%#x), want %v (%#x)", x, frac, math.Float64bits(frac), goFrac, math.Float64bits(goFrac))
		}

		// float32(x) can overflow a large finite double to +-Inf, where Modff
		// follows POSIX (frac=+-0) and intentionally differs from Go's math.Modf
		// (frac=NaN) — those specials are checked in TestRoundModfInfNaN, so the
		// finite cross-check against Go skips them.
		fx := float32(x)
		if !math.IsInf(float64(fx), 0) {
			gI, gF := math.Modf(float64(fx))
			var ipf float32
			fracf := Modff(fx, &ipf)
			if !f32eq(ipf, float32(gI)) {
				t.Errorf("Modff(%v) iptr = %v, want %v", fx, ipf, float32(gI))
			}
			if !f32eq(fracf, float32(gF)) {
				t.Errorf("Modff(%v) frac = %v, want %v", fx, fracf, float32(gF))
			}
		}
	}
}

// POSIX edge semantics that intentionally differ from Go's math.Modf.
func TestRoundModfInfNaN(t *testing.T) {
	for _, s := range []float64{math.Inf(1), math.Inf(-1)} {
		var ip float64
		frac := Modf(s, &ip)
		if !f64eq(ip, s) {
			t.Errorf("Modf(%v) iptr = %v, want %v", s, ip, s)
		}
		if !f64eq(frac, math.Copysign(0, s)) {
			t.Errorf("Modf(%v) frac = %v, want signed 0", s, frac)
		}
	}
	nan := math.NaN()
	var ip float64
	frac := Modf(nan, &ip)
	if !math.IsNaN(ip) || !math.IsNaN(frac) {
		t.Errorf("Modf(NaN) = (%v, %v), want (NaN, NaN)", frac, ip)
	}

	// Modff mirrors the same POSIX specials.
	for _, s := range []float32{float32(math.Inf(1)), float32(math.Inf(-1))} {
		var ipf float32
		fracf := Modff(s, &ipf)
		if !f32eq(ipf, s) {
			t.Errorf("Modff(%v) iptr = %v, want %v", s, ipf, s)
		}
		if !f32eq(fracf, float32(math.Copysign(0, float64(s)))) {
			t.Errorf("Modff(%v) frac = %v, want signed 0", s, fracf)
		}
	}
	var ipf float32
	fracf := Modff(float32(nan), &ipf)
	if ipf == ipf || fracf == fracf { // NaN != NaN
		t.Errorf("Modff(NaN) = (%v, %v), want (NaN, NaN)", fracf, ipf)
	}
}

// fmod / remainder / remquo and f-variants (musl/src/math, wave 1). fmod is the
// truncated remainder (== Go math.Mod, sign of x); remainder is the IEEE-754
// round-half-even remainder (== Go math.Remainder). remquo returns that same
// remainder and additionally the sign + low bits of the integer quotient.
var modPairs = []struct{ x, y float64 }{
	{5, 3}, {-5, 3}, {5, -3}, {-5, -3},
	{6, 3}, {-6, 3}, {-6, -3}, // exact multiples -> signed-zero result
	{5.5, 2}, {-5.5, 2}, {1, 3}, {3, 3},
	{7.25, 0.5}, {100.5, 0.3}, {1e10 + 0.5, 3},
	{2.3, 1.1}, {-2.3, 1.1}, {123456.789, 1000},
	{2.2250738585072014e-308, 1e-309}, // subnormal reduction
	{1e300, 7}, {math.Pi, math.E},
	{0.5, 0.5}, {1.5, 0.5}, {0.3, 100},
}

func TestFmod(t *testing.T) {
	for _, p := range modPairs {
		if got, want := Fmod(p.x, p.y), math.Mod(p.x, p.y); !f64eq(got, want) {
			t.Errorf("Fmod(%v,%v) = %v (%#x), want %v (%#x)", p.x, p.y, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		fx, fy := float32(p.x), float32(p.y)
		if got, want := Fmodf(fx, fy), float32(math.Mod(float64(fx), float64(fy))); !f32eq(got, want) {
			t.Errorf("Fmodf(%v,%v) = %v, want %v", fx, fy, got, want)
		}
	}
	// POSIX specials: fmod(x,0)=NaN, fmod(±Inf,y)=NaN, fmod(x,±Inf)=x, fmod(x,NaN)=NaN.
	if !math.IsNaN(Fmod(1, 0)) {
		t.Error("Fmod(1,0) not NaN")
	}
	if !math.IsNaN(Fmod(math.Inf(1), 3)) {
		t.Error("Fmod(Inf,3) not NaN")
	}
	if got := Fmod(3, math.Inf(1)); !f64eq(got, 3) {
		t.Errorf("Fmod(3,Inf) = %v, want 3", got)
	}
	if !math.IsNaN(Fmod(3, math.NaN())) {
		t.Error("Fmod(3,NaN) not NaN")
	}
}

func TestRemainderRemquo(t *testing.T) {
	for _, p := range modPairs {
		if got, want := Remainder(p.x, p.y), math.Remainder(p.x, p.y); !f64eq(got, want) {
			t.Errorf("Remainder(%v,%v) = %v (%#x), want %v (%#x)", p.x, p.y, got, math.Float64bits(got), want, math.Float64bits(want))
		}
		fx, fy := float32(p.x), float32(p.y)
		if got, want := Remainderf(fx, fy), float32(math.Remainder(float64(fx), float64(fy))); !f32eq(got, want) {
			t.Errorf("Remainderf(%v,%v) = %v, want %v", fx, fy, got, want)
		}

		// remquo: the remainder matches, and quo carries the integer quotient's
		// sign + low 3 bits. The exact signed integer quotient is (x-rem)/y.
		var q int32
		rem := Remquo(p.x, p.y, &q)
		if !f64eq(rem, math.Remainder(p.x, p.y)) {
			t.Errorf("Remquo(%v,%v) rem = %v, want %v", p.x, p.y, rem, math.Remainder(p.x, p.y))
		}
		qf := (p.x - rem) / p.y
		if qf != 0 && (q < 0) != math.Signbit(qf) {
			t.Errorf("Remquo(%v,%v) quo=%d sign, want signbit %v", p.x, p.y, q, math.Signbit(qf))
		}
		if a := math.Abs(qf); a < (1<<52) && a == math.Trunc(a) {
			got3 := int64(q)
			if got3 < 0 {
				got3 = -got3
			}
			if got3&7 != int64(a)&7 {
				t.Errorf("Remquo(%v,%v) quo low3 = %d, want %d", p.x, p.y, got3&7, int64(a)&7)
			}
		}
	}
	// remquo(x,0) -> NaN remainder, quo unspecified but musl sets 0.
	var q int32
	if !math.IsNaN(Remquo(1, 0, &q)) {
		t.Error("Remquo(1,0) rem not NaN")
	}
}
