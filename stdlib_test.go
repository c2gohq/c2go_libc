package libc

import "testing"

func TestStdlibArith(t *testing.T) {
	if Abs(-5) != 5 || Abs(5) != 5 {
		t.Fatal("abs")
	}
	if Labs(-5) != 5 || Llabs(-7) != 7 || Imaxabs(-9) != 9 {
		t.Fatal("labs/llabs/imaxabs")
	}
	if q, r := Div(17, 5); q != 3 || r != 2 {
		t.Fatalf("div(17,5)=%d,%d want 3,2", q, r)
	}
	if q, r := Ldiv(-17, 5); q != -3 || r != -2 { // C truncates toward zero
		t.Fatalf("ldiv(-17,5)=%d,%d want -3,-2", q, r)
	}
}

func TestStdlibAtox(t *testing.T) {
	if Atoi(csb("42")) != 42 || Atoi(csb("-7")) != -7 || Atoi(csb("  13x")) != 13 {
		t.Fatal("atoi")
	}
	if Atol(csb("123456")) != 123456 {
		t.Fatal("atol")
	}
	if Atoll(csb("9999999999")) != 9999999999 {
		t.Fatal("atoll")
	}
}

func TestPrng(t *testing.T) {
	Srand(1)
	a1, a2 := Rand(), Rand()
	Srand(1)
	b1, b2 := Rand(), Rand()
	if a1 != b1 || a2 != b2 {
		t.Fatal("srand/rand not deterministic")
	}
	if a1 < 0 || a2 < 0 {
		t.Fatal("rand negative")
	}
	s := uint32(12345)
	if RandR(&s) < 0 {
		t.Fatal("rand_r negative")
	}
}
