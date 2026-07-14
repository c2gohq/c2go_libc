package libc

import "testing"

func TestWctype(t *testing.T) {
	if Iswalpha('a') == 0 || Iswalpha('Z') == 0 || Iswalpha('1') != 0 {
		t.Fatal("iswalpha")
	}
	if Iswdigit('7') == 0 || Iswdigit('x') != 0 || Iswspace(' ') == 0 || Iswspace('x') != 0 {
		t.Fatal("iswdigit/iswspace")
	}
	if Towlower('A') != 'a' || Towupper('z') != 'Z' || Towlower('5') != '5' {
		t.Fatal("towlower/towupper")
	}
	if Wcwidth('A') != 1 || Wcwidth(0) != 0 {
		t.Fatal("wcwidth")
	}
	// wctype("alpha") -> class id; iswctype dispatches on it
	al := Wctype(cstr("alpha"))
	if al == 0 || Iswctype('a', al) == 0 || Iswctype('1', al) != 0 {
		t.Fatal("wctype/iswctype")
	}
	ws := []int32{'A', 'B', 'C', 0}
	if Wcswidth(&ws[0], 3) != 3 {
		t.Fatal("wcswidth")
	}
	// wctrans("tolower") -> transform id; towctrans applies it
	if tr := Wctrans(cstr("tolower")); Towctrans('A', tr) != 'a' {
		t.Fatal("wctrans/towctrans")
	}
}
