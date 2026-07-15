// wctype.go — the non-ASCII half of the wide ctype (source/wctype.c).
//
// c2go-libc's wctype is a C/Go hybrid. Everything table-free — the arithmetic
// classes (iswdigit/iswcntrl/iswprint/...), the composites (iswalnum/iswgraph/
// iswpunct/iswlower/iswupper), the meta dispatch (iswctype/wctype/towctrans/
// wctrans) and the ASCII fast path of every function — is C, ported from musl
// (source/wctype.c). Only the three irreducibly table-driven lookups keep their
// ASCII path in C and delegate their NON-ASCII (wc >= 128) path here, to Go's
// `unicode` package, rather than vendoring musl's ~39 KB alpha/casemap tables.
// This mirrors the iconv decision: a big Unicode table is Go's to own, not ours
// to duplicate. The bridge is pure scalar (wint_t in, int/wint_t out) — no
// pointer crosses the c2go boundary, so there is no GC/copystack hazard.
//
// Consequence: non-ASCII classification/case follows Go's Unicode version and
// category choices, not musl's tables. Most visibly iswalpha uses unicode.Letter
// (general category L), which differs from musl's Alphabetic property (L + Nl +
// Other_Alphabetic) for a few code points (e.g. Roman numerals U+2160+). ASCII
// is POSIX-exact via the C narrow ctype.

package libc

import (
	"unicode"
	_ "unsafe" // for //go:linkname
)

// __c2go_uni_alpha reports whether wc (only ever called with wc >= 128) is a
// Unicode letter. Returns 1/0 (C int).
//
//go:linkname __c2go_uni_alpha
func __c2go_uni_alpha(wc uint32) int32 {
	if unicode.IsLetter(rune(wc)) {
		return 1
	}
	return 0
}

// __c2go_uni_tolower is the simple (1:1) Unicode lowercase map for wc >= 128;
// it returns wc unchanged when there is no mapping.
//
//go:linkname __c2go_uni_tolower
func __c2go_uni_tolower(wc uint32) uint32 {
	return uint32(unicode.ToLower(rune(wc)))
}

// __c2go_uni_toupper is the simple (1:1) Unicode uppercase map for wc >= 128.
//
//go:linkname __c2go_uni_toupper
func __c2go_uni_toupper(wc uint32) uint32 {
	return uint32(unicode.ToUpper(rune(wc)))
}
