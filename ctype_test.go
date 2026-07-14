package libc

import "testing"

func TestCtype(t *testing.T) {
	if Isalpha('a') == 0 || Isalpha('Z') == 0 || Isalpha('1') != 0 {
		t.Fatal("isalpha")
	}
	if Isdigit('7') == 0 || Isdigit('x') != 0 {
		t.Fatal("isdigit")
	}
	if Isalnum('z') == 0 || Isalnum('!') != 0 {
		t.Fatal("isalnum")
	}
	if Isspace(' ') == 0 || Isspace('\t') == 0 || Isspace('x') != 0 {
		t.Fatal("isspace")
	}
	if Isxdigit('f') == 0 || Isxdigit('g') != 0 {
		t.Fatal("isxdigit")
	}
	if Ispunct('!') == 0 || Ispunct('a') != 0 {
		t.Fatal("ispunct")
	}
	if Isupper('A') == 0 || Isupper('a') != 0 || Islower('a') == 0 {
		t.Fatal("case class")
	}
	if Toupper('a') != 'A' || Tolower('Z') != 'z' || Toupper('5') != '5' {
		t.Fatal("case conv")
	}
	if Isascii(0x41) == 0 || Isascii(0x80) != 0 {
		t.Fatal("isascii")
	}
	if Toascii(0x1c1) != 0x41 {
		t.Fatal("toascii")
	}
	// _l variants ignore their locale (C locale only)
	if IsalphaL('a', nil) == 0 || ToupperL('a', nil) != 'A' {
		t.Fatal("_l variants")
	}
}
