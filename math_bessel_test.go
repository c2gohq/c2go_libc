// math_bessel_test.go — Bessel j0/y0/j1/y1/jn/yn and exp10/pow10. Reference
// values come from the host system libm (FreeBSD/Sun msun — the same source
// musl's j0/j1/jn are ported from), captured at 17 significant digits; the
// tolerance (1e-12 relative) covers the few-ULP spread between the two builds
// while still catching any transcription error in the ported polynomial tables.

package libc

import (
	"math"
	"testing"
)

func approx(t *testing.T, name string, got, want float64) {
	t.Helper()
	if math.Abs(got-want) > 1e-12*math.Max(1, math.Abs(want)) {
		t.Errorf("%s = %.17g, want %.17g", name, got, want)
	}
}

func TestBessel(t *testing.T) {
	approx(t, "j0(0.5)", J0(0.5), 0.93846980724081286)
	approx(t, "j0(1)", J0(1), 0.76519768655796661)
	approx(t, "j0(2)", J0(2), 0.22389077914123567)
	approx(t, "j0(5)", J0(5), -0.17759677131433829)
	approx(t, "j0(10)", J0(10), -0.24593576445134829)
	approx(t, "j1(0.5)", J1(0.5), 0.2422684576748739)
	approx(t, "j1(1)", J1(1), 0.4400505857449335)
	approx(t, "j1(2)", J1(2), 0.57672480775687329)
	approx(t, "j1(5)", J1(5), -0.32757913759146523)
	approx(t, "jn(2,1)", Jn(2, 1), 0.11490348493190049)
	approx(t, "jn(3,5)", Jn(3, 5), 0.36483123061366701)
	approx(t, "jn(5,10)", Jn(5, 10), -0.2340615281867936)
	approx(t, "jn(0,3)", Jn(0, 3), -0.2600519549019335)  // == j0(3)
	approx(t, "jn(1,3)", Jn(1, 3), 0.33905895852593643)  // == j1(3)
	approx(t, "y0(1)", Y0(1), 0.08825696421567697)
	approx(t, "y0(2)", Y0(2), 0.51037567264974504)
	approx(t, "y0(5)", Y0(5), -0.30851762524903376)
	approx(t, "y1(1)", Y1(1), -0.78121282130028868)
	approx(t, "y1(2)", Y1(2), -0.10703243154093756)
	approx(t, "yn(2,1)", Yn(2, 1), -1.6506826068162543)
	approx(t, "yn(3,5)", Yn(3, 5), 0.14626716269319279)

	// special cases (exact)
	if J0(0) != 1 {
		t.Errorf("j0(0) = %v, want 1", J0(0))
	}
	if J1(0) != 0 {
		t.Errorf("j1(0) = %v, want 0", J1(0))
	}
	if !math.IsInf(Y0(0), -1) {
		t.Errorf("y0(0) = %v, want -Inf", Y0(0))
	}
	if !math.IsNaN(Y0(-1)) {
		t.Errorf("y0(-1) = %v, want NaN", Y0(-1))
	}
	if !math.IsNaN(J0(math.NaN())) {
		t.Errorf("j0(NaN) should be NaN")
	}
}

func TestExp10(t *testing.T) {
	approx(t, "exp10(0)", Exp10(0), 1)
	approx(t, "exp10(3)", Exp10(3), 1000)
	approx(t, "exp10(-2)", Exp10(-2), 0.01)
	approx(t, "exp10(0.5)", Exp10(0.5), 3.1622776601683795)
	approx(t, "exp10(20)", Exp10(20), 1e20)  // |n|>=16 -> pow(10,x) path
	approx(t, "exp10(-15)", Exp10(-15), 1e-15)
	approx(t, "pow10(2)", Pow10(2), 100)
	// integer exponents inside the table are returned exactly
	if Exp10(3) != 1000 {
		t.Errorf("exp10(3) = %.17g, want exactly 1000", Exp10(3))
	}
}
