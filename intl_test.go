package libc

// #652 — the locale tail: the gettext compatibility layer (no catalogs and
// only the C.UTF-8 locale → every lookup is plural selection between the
// caller's own msgids, while textdomain/bindtextdomain keep real state) and
// strfmon (the C locale defines no currency symbol/grouping → musl's plain
// numeric formatting).

import (
	"runtime"
	"testing"
)

// TestGettextFamily: with no catalogs every variant returns the caller's own
// msgid POINTER (identity, not just content), selecting msgid2 iff n != 1.
func TestGettextFamily(t *testing.T) {
	msg := csb("hello")
	if got := Gettext(msg); got != msg {
		t.Errorf("Gettext = %p, want the msgid pointer %p", got, msg)
	}
	one, many := csb("1 file"), csb("%d files")
	if got := Ngettext(one, many, 1); got != one {
		t.Errorf("Ngettext(n=1) = %q, want msgid1", cstr(got))
	}
	if got := Ngettext(one, many, 2); got != many {
		t.Errorf("Ngettext(n=2) = %q, want msgid2", cstr(got))
	}
	if got := Ngettext(one, many, 0); got != many {
		t.Errorf("Ngettext(n=0) = %q, want msgid2", cstr(got))
	}
	// Domain and category are ignored — even out-of-range categories.
	if got := Dgettext(csb("dom"), msg); got != msg {
		t.Errorf("Dgettext = %q, want the msgid", cstr(got))
	}
	if got := Dcgettext(nil, msg, 99); got != msg {
		t.Errorf("Dcgettext(cat=99) = %q, want the msgid", cstr(got))
	}
	if got := Dngettext(csb("dom"), one, many, 1); got != one {
		t.Errorf("Dngettext(n=1) = %q, want msgid1", cstr(got))
	}
	if got := Dcngettext(nil, one, many, 5, 99); got != many {
		t.Errorf("Dcngettext(n=5) = %q, want msgid2", cstr(got))
	}
}

// TestTextdomain: default "messages"; a set domain is stored and queryable;
// an overlong name (> NAME_MAX) fails with EINVAL.
func TestTextdomain(t *testing.T) {
	if got := cstr(Textdomain(nil)); got != "messages" {
		t.Errorf("default domain = %q, want messages", got)
	}
	if got := cstr(Textdomain(csb("mydom"))); got != "mydom" {
		t.Errorf("Textdomain(mydom) = %q", got)
	}
	if got := cstr(Textdomain(nil)); got != "mydom" {
		t.Errorf("domain after set = %q, want mydom", got)
	}
	long := make([]byte, 300) // NAME_MAX is 255 on all targets
	for i := range long {
		long[i] = 'a'
	}
	long[299] = 0
	*ErrnoPtr() = 0
	if got := Textdomain(&long[0]); got != nil {
		t.Errorf("overlong Textdomain = %q, want NULL", cstr(got))
	}
	if e := *ErrnoPtr(); e != errEINVAL {
		t.Errorf("errno = %d, want EINVAL(%d)", e, errEINVAL)
	}
}

// TestBindtextdomain: NULL-dirname queries the stored binding; rebinding a
// domain replaces the visible directory; a NULL domain is rejected.
func TestBindtextdomain(t *testing.T) {
	dom := csb("intl-test-dom")
	if got := Bindtextdomain(dom, nil); got != nil {
		t.Errorf("unbound query = %q, want NULL", cstr(got))
	}
	if got := cstr(Bindtextdomain(dom, csb("/usr/share/locale"))); got != "/usr/share/locale" {
		t.Errorf("bind = %q", got)
	}
	if got := cstr(Bindtextdomain(dom, nil)); got != "/usr/share/locale" {
		t.Errorf("query after bind = %q", got)
	}
	if got := cstr(Bindtextdomain(dom, csb("/opt/locale"))); got != "/opt/locale" {
		t.Errorf("rebind = %q", got)
	}
	if got := cstr(Bindtextdomain(dom, nil)); got != "/opt/locale" {
		t.Errorf("query after rebind = %q", got)
	}
	if got := Bindtextdomain(nil, csb("/x")); got != nil {
		t.Errorf("NULL-domain bind = %q, want NULL", cstr(got))
	}
}

// TestBindTextdomainCodeset: only UTF-8 (case-insensitive) is accepted; any
// other codeset is EINVAL; a NULL codeset just queries.
func TestBindTextdomainCodeset(t *testing.T) {
	if got := cstr(BindTextdomainCodeset(csb("dom"), csb("utf-8"))); got != "UTF-8" {
		t.Errorf("codeset(utf-8) = %q, want UTF-8", got)
	}
	if got := cstr(BindTextdomainCodeset(csb("dom"), nil)); got != "UTF-8" {
		t.Errorf("codeset(NULL) = %q, want UTF-8", got)
	}
	*ErrnoPtr() = 0
	if got := BindTextdomainCodeset(csb("dom"), csb("ISO-8859-1")); got != nil {
		t.Errorf("codeset(ISO-8859-1) = %q, want NULL", cstr(got))
	}
	if e := *ErrnoPtr(); e != errEINVAL {
		t.Errorf("errno = %d, want EINVAL(%d)", e, errEINVAL)
	}
}

// sfm formats via strfmon into a bufLen-byte buffer (doubles only — the only
// argument type strfmon consumes).
func sfm(t *testing.T, bufLen int, format string, vals ...float64) (string, int64) {
	t.Helper()
	buf := make([]byte, bufLen)
	fb := append([]byte(format), 0)
	a := &pargs{}
	for _, v := range vals {
		a.f(v)
	}
	ap, ptrs := a.packPtr()
	n := Strfmon(&buf[0], uint64(len(buf)), &fb[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(fb)
	if n < 0 {
		return "", n
	}
	return string(buf[:n]), n
}

// TestStrfmon: C-locale monetary formatting is "%*.*f" with width from the
// field/left-precision spec (musl behavior); an overflowing result is E2BIG.
func TestStrfmon(t *testing.T) {
	cases := []struct {
		f    string
		v    []float64
		want string
	}{
		{"%n", []float64{1234.567}, "1234.57"},
		{"%11n", []float64{1234.567}, "    1234.57"},
		{"%#5.0n", []float64{1234.567}, "  1235"},
		{"%.3i", []float64{-1.5}, "-1.500"},
		{"cost: %n!", []float64{9.9}, "cost: 9.90!"},
		{"100%%", nil, "100%"},
		{"a=%n b=%n", []float64{1.0, 2.5}, "a=1.00 b=2.50"},
	}
	for _, c := range cases {
		got, n := sfm(t, 64, c.f, c.v...)
		if got != c.want {
			t.Errorf("strfmon(%q) = %q (n=%d), want %q", c.f, got, n, c.want)
		}
	}

	*ErrnoPtr() = 0
	if _, n := sfm(t, 4, "%n", 1234.567); n != -1 {
		t.Errorf("overflowing strfmon = %d, want -1", n)
	}
	if e := *ErrnoPtr(); e != 7 { // E2BIG == 7 on all three targets
		t.Errorf("errno = %d, want E2BIG(7)", e)
	}
}

// TestStrfmonL: the _l variant ignores its locale_t (only one locale exists).
func TestStrfmonL(t *testing.T) {
	buf := make([]byte, 64)
	fb := append([]byte("%n"), 0)
	a := &pargs{}
	a.f(42.5)
	ap, ptrs := a.packPtr()
	n := StrfmonL(&buf[0], uint64(len(buf)), nil, &fb[0], ap)
	runtime.KeepAlive(a)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(fb)
	if n != 5 || string(buf[:max(n, 0)]) != "42.50" {
		t.Errorf("StrfmonL = %d %q, want 5 %q", n, string(buf[:max(n, 0)]), "42.50")
	}
}
