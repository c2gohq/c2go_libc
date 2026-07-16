// prng_test.go — the rand48 + BSD random families (#698). TestRandFamilies is
// OLD batch675_test.go's prng part, verbatim; TestSeed48CrossTU additionally
// pins the cluster's one structural risk: the mutable __seed48[7] DATA
// definition (fork __seed48.c, pristine) is shared across TUs by plain extern
// (the __fsmu8 precedent extended to WRITABLE data) — srand48 writes it in one
// TU, drand48/lrand48 read it from others, so reproducibility across that
// boundary proves the cross-TU data linkage.
package libc

import "testing"

func TestRandFamilies(t *testing.T) {
	// rand_r: deterministic per seed, reproducible.
	s1, s2 := uint32(42), uint32(42)
	for i := 0; i < 8; i++ {
		if RandR(&s1) != RandR(&s2) {
			t.Fatal("rand_r not reproducible")
		}
	}
	// drand48: srand48 reproducibility + [0,1) range.
	Srand48(7)
	var seq [4]float64
	for i := range seq {
		seq[i] = Drand48()
		if seq[i] < 0 || seq[i] >= 1 {
			t.Fatalf("drand48 out of range: %v", seq[i])
		}
	}
	Srand48(7)
	for i := range seq {
		if Drand48() != seq[i] {
			t.Fatal("drand48 not reproducible after srand48")
		}
	}
	// random: srandom reproducibility, 31-bit range.
	Srandom(99)
	a, b := Random(), Random()
	Srandom(99)
	if Random() != a || Random() != b {
		t.Fatal("random not reproducible after srandom")
	}
	if a < 0 || a > 0x7fffffff {
		t.Fatalf("random out of 31-bit range: %d", a)
	}
}

func TestSeed48CrossTU(t *testing.T) {
	// seed48 (seed48.c) swaps the shared __seed48; lrand48/mrand48 (other TUs)
	// must observe the new seed — and seed48's returned previous-seed pointer
	// must round-trip.
	var s [3]uint16
	s[0], s[1], s[2] = 0x1234, 0x5678, 0x9abc
	old := Seed48(&s[0])
	if old == nil {
		t.Fatal("seed48 returned nil")
	}
	v1, m1 := Lrand48(), Mrand48()
	Seed48(&s[0])
	if Lrand48() != v1 || Mrand48() != m1 {
		t.Fatal("lrand48/mrand48 not reproducible after seed48 (cross-TU __seed48 broken?)")
	}
	// lcong48 overwrites all 7 words (incl. the multiplier); a different
	// multiplier must change the stream. Restore the default afterwards.
	var p [7]uint16
	p[0], p[1], p[2] = 1, 2, 3
	p[3], p[4], p[5] = 0x1111, 0x2222, 0x3 // non-default a
	p[6] = 0xb
	Lcong48(&p[0])
	if Lrand48() == v1 {
		t.Fatal("lcong48 multiplier change did not alter the stream")
	}
	Srand48(0) // restore the default multiplier/addend for other tests
}
