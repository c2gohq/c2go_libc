//go:build unix

package libc

// nl_langinfo (#648): the C locale's fixed answers, item encoding
// (category << 16 | index) per <langinfo.h>/musl. (unix build tag only to
// stay off the pre-existing windows test-vet surface — the implementation
// itself is OS-independent and linked on Windows too.)

import "testing"

func TestNlLanginfo(t *testing.T) {
	cases := []struct {
		item int32
		want string
	}{
		{14, "UTF-8"},               // CODESET
		{0x10000, "."},              // RADIXCHAR
		{0x10001, ""},               // THOUSEP
		{0x20000, "Sun"},            // ABDAY_1
		{0x20006, "Sat"},            // ABDAY_7
		{0x20007, "Sunday"},         // DAY_1
		{0x2000E, "Jan"},            // ABMON_1
		{0x20019, "Dec"},            // ABMON_12
		{0x2001A, "January"},        // MON_1
		{0x20025, "December"},       // MON_12
		{0x20026, "AM"},             // AM_STR
		{0x20027, "PM"},             // PM_STR
		{0x20028, "%a %b %e %T %Y"}, // D_T_FMT
		{0x20029, "%m/%d/%y"},       // D_FMT
		{0x2002A, "%H:%M:%S"},       // T_FMT
		{0x2002B, "%I:%M:%S %p"},    // T_FMT_AMPM
		{0x2002C, ""},               // ERA
		{0x4000F, ""},               // CRNCYSTR
		{0x50000, "^[yY]"},          // YESEXPR
		{0x50001, "^[nN]"},          // NOEXPR
		{0x70123, ""},               // unknown category → ""
	}
	for _, c := range cases {
		got := cstr(NlLanginfo(c.item))
		if got != c.want {
			t.Errorf("nl_langinfo(%#x) = %q, want %q", c.item, got, c.want)
		}
	}
	// The _l variant ignores the locale (only C exists).
	if got := cstr(NlLanginfoL(14, nil)); got != "UTF-8" {
		t.Errorf("nl_langinfo_l(CODESET) = %q, want UTF-8", got)
	}
}
