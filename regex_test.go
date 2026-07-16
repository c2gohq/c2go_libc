//go:build !windows

package libc

// POSIX regex (TRE) ABI smoke test, extracted verbatim from OLD regex_test.go
// (#667 — its fnmatch/glob parts arrived with the dirent cluster,
// fnmatch_glob_test.go). Semantic surface is covered by the dual-arch dual-O
// probe harness vs the host-libc oracle (probe_regex, #667 log). regex_t is a
// 0-byte opaque binding: allocate real backing and cast (quickbatch precedent).

import (
	"testing"
	"unsafe"
)

// regexT is 64-byte backing for the opaque regex_t (musl: size_t + 5 ptrs +
// size_t + char, padded to 64 on LP64).
type regexT [8]uint64

// regMatch mirrors regmatch_t { regoff_t rm_so, rm_eo } (regoff_t = int64).
type regMatch struct{ so, eo int64 }

func TestRegexCompExecError(t *testing.T) {
	var re regexT
	preg := (*re_pattern_buffer)(unsafe.Pointer(&re))
	if rc := Regcomp(preg, csb("a(b+)c"), 1 /* REG_EXTENDED */); rc != 0 {
		t.Fatalf("regcomp = %d, want 0", rc)
	}
	var m [4]regMatch
	rc := Regexec(preg, csb("xxabbbcyy"), 4, (*regmatch_t)(unsafe.Pointer(&m[0])), 0)
	if rc != 0 {
		t.Fatalf("regexec = %d, want 0", rc)
	}
	if m[0].so != 2 || m[0].eo != 7 || m[1].so != 3 || m[1].eo != 6 {
		t.Fatalf("submatches = %v, want m0=(2,7) m1=(3,6)", m[:2])
	}
	if rc := Regexec(preg, csb("xxacyy"), 0, nil, 0); rc != 1 { // REG_NOMATCH
		t.Fatalf("regexec nomatch = %d, want 1", rc)
	}
	Regfree(preg)

	// bad pattern -> REG_EBRACK(7); regerror returns strlen+1 and the musl text
	var re2 regexT
	preg2 := (*re_pattern_buffer)(unsafe.Pointer(&re2))
	rc = Regcomp(preg2, csb("[abc"), 1)
	if rc != 7 {
		t.Fatalf("regcomp([abc) = %d, want REG_EBRACK(7)", rc)
	}
	var buf [64]byte
	n := Regerror(rc, preg2, &buf[0], 64)
	if got := cstr(&buf[0]); got != "Missing ']'" || n != uint64(len(got)+1) {
		t.Fatalf("regerror = %q/%d, want \"Missing ']'\"/12", got, n)
	}
}
