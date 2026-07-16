package libc

// iconv_test drives the Go-bridge iconv (iconv.go) through its POSIX
// **byte/*uint64 signatures and cross-checks every conversion against a direct
// golang.org/x/text transform. Buffers are forced onto the heap (esc) so their
// backing-array addresses stay stable across the bridge call, mirroring how the
// multibyte/wstring tests hold their slices.

import (
	"bytes"
	"testing"
	"unsafe"

	"golang.org/x/text/encoding/charmap"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/encoding/traditionalchinese"
	"golang.org/x/text/encoding/unicode"
)

// iconvSink roots the escaped slices so escape analysis heap-allocates them.
var iconvSink any

// esc forces b's backing array to the heap (stable address across the call).
func esc(b []byte) []byte { iconvSink = &b; return b }

// cstrOf builds a NUL-terminated C string (*byte) from a Go string.
func cstrOf(s string) *byte {
	b := esc(append([]byte(s), 0))
	return &b[0]
}

const iconvErr = ^uint64(0) // (size_t)-1

// iconvBad is the GO-side failure value: nil. (POSIX's (iconv_t)-1 sentinel
// exists only in the C world — source/iconv.c maps nil↔-1 at the boundary; a
// manufactured -1 in a Go pointer slot would trip the precise stack scan.)
func iconvBad() unsafe.Pointer { return nil }

// checkConv opens to<-from, converts src in one shot with an ample output
// buffer, and asserts a full, faithful conversion.
func checkConv(t *testing.T, to, from string, src, want []byte) {
	t.Helper()
	cd := IconvOpen(cstrOf(to), cstrOf(from))
	if cd == iconvBad() {
		t.Fatalf("iconv_open(%q,%q) failed unexpectedly", to, from)
	}
	in := esc(append([]byte(nil), src...))
	out := esc(make([]byte, len(want)+64))
	inp, op := &in[0], &out[0]
	inleft, outleft := uint64(len(in)), uint64(len(out))
	*ErrnoPtr() = 0

	r := Iconv(cd, &inp, &inleft, &op, &outleft)
	if r == iconvErr {
		t.Fatalf("iconv(%s<-%s) = -1, errno=%d", to, from, *ErrnoPtr())
	}
	if inleft != 0 {
		t.Fatalf("iconv(%s<-%s) inleft=%d, want 0 (input not fully consumed)", to, from, inleft)
	}
	// inbuf must have advanced to exactly one-past-the-last input byte.
	if got, wantp := uintptr(unsafe.Pointer(inp)), uintptr(unsafe.Pointer(&in[0]))+uintptr(len(in)); got != wantp {
		t.Fatalf("iconv(%s<-%s) inbuf advanced to %#x, want %#x", to, from, got, wantp)
	}
	produced := uint64(len(out)) - outleft
	if got := out[:produced]; !bytes.Equal(got, want) {
		t.Fatalf("iconv(%s<-%s) = % x, want % x (x/text reference)", to, from, got, want)
	}
	if rc := IconvClose(cd); rc != 0 {
		t.Fatalf("iconv_close = %d, want 0", rc)
	}
}

// TestIconvRoundTrip covers UTF-8 <-> {GBK, Big5, UTF-16LE, ISO-8859-1}, both
// directions, each cross-checked against a direct x/text transform.
func TestIconvRoundTrip(t *testing.T) {
	han := "你好，世界"                   // Simplified Chinese
	zht := "臺灣繁體字"                   // Traditional Chinese (Big5)
	lat := "Résumé £5 — naïve café" // Latin-1-representable text

	gbk, err := simplifiedchinese.GBK.NewEncoder().Bytes([]byte(han))
	if err != nil {
		t.Fatalf("reference GBK encode: %v", err)
	}
	checkConv(t, "GBK", "UTF-8", []byte(han), gbk)
	checkConv(t, "UTF-8", "GBK", gbk, []byte(han))

	big5, err := traditionalchinese.Big5.NewEncoder().Bytes([]byte(zht))
	if err != nil {
		t.Fatalf("reference Big5 encode: %v", err)
	}
	checkConv(t, "BIG5", "UTF-8", []byte(zht), big5)
	checkConv(t, "UTF-8", "BIG5", big5, []byte(zht))

	u16, err := unicode.UTF16(unicode.LittleEndian, unicode.IgnoreBOM).NewEncoder().Bytes([]byte(han))
	if err != nil {
		t.Fatalf("reference UTF-16LE encode: %v", err)
	}
	checkConv(t, "UTF-16LE", "UTF-8", []byte(han), u16)
	checkConv(t, "UTF-8", "UTF-16LE", u16, []byte(han))

	// "—" is NOT Latin-1; strip it for the Latin-1 vectors.
	lat = "Résumé £5 naïve café"
	iso, err := charmap.ISO8859_1.NewEncoder().Bytes([]byte(lat))
	if err != nil {
		t.Fatalf("reference ISO-8859-1 encode: %v", err)
	}
	checkConv(t, "ISO-8859-1", "UTF-8", []byte(lat), iso)
	checkConv(t, "UTF-8", "ISO-8859-1", iso, []byte(lat))
}

// TestIconvE2BIGDrain drives the *standard POSIX iconv loop* — a fixed, tiny
// output window drained on every E2BIG — and asserts the FULL output is
// recovered. This is the test that catches silent truncation: a transformer that
// eagerly consumes all input on the first call (reporting inleft→0 while emitting
// only one window) makes the loop exit early with a truncated result. A single
// faithful transformer instead advances inbuf by exactly the chars whose output
// fit, so the loop converges on the complete conversion.
func TestIconvE2BIGDrain(t *testing.T) {
	han := "你好世界" // 12 UTF-8 bytes -> 8 GBK bytes (four 2-byte chars)
	want, err := simplifiedchinese.GBK.NewEncoder().Bytes([]byte(han))
	if err != nil {
		t.Fatalf("reference GBK encode: %v", err)
	}
	cd := IconvOpen(cstrOf("GBK"), cstrOf("UTF-8"))
	if cd == iconvBad() {
		t.Fatal("iconv_open(GBK,UTF-8) failed")
	}
	defer IconvClose(cd)

	in := esc(append([]byte(nil), []byte(han)...))
	inp := &in[0]
	inleft := uint64(len(in))
	window := esc(make([]byte, 2)) // 2-byte window: forces repeated E2BIG

	var got []byte
	sawE2BIG := false
	for iter := 0; inleft > 0; iter++ {
		if iter > len(in) {
			t.Fatalf("iconv drain did not converge: inleft=%d after %d iters", inleft, iter)
		}
		op := &window[0]
		outleft := uint64(len(window))
		*ErrnoPtr() = 0
		r := Iconv(cd, &inp, &inleft, &op, &outleft)
		got = append(got, window[:uint64(len(window))-outleft]...)
		if r == iconvErr {
			if e := *ErrnoPtr(); e != errE2BIG {
				t.Fatalf("iconv drain: r=-1 errno=%d, want E2BIG(%d)", e, errE2BIG)
			}
			sawE2BIG = true // flush the window (done above) and continue
			continue
		}
	}
	if !sawE2BIG {
		t.Fatal("expected at least one E2BIG with a 2-byte window")
	}
	if !bytes.Equal(got, want) {
		t.Fatalf("iconv drain = % x, want % x (silent truncation?)", got, want)
	}
}

// TestIconvEINVALSplit feeds an INCOMPLETE trailing multibyte sequence and
// asserts (size_t)-1 + EINVAL with inbuf left AT the lead byte — the bytes are
// preserved for a later call, not silently swallowed. (A transform.Chain whose
// first stage is an identity pass-through consumes the incomplete tail and
// advances inbuf past it, which this test rejects.)
func TestIconvEINVALSplit(t *testing.T) {
	cd := IconvOpen(cstrOf("GBK"), cstrOf("UTF-8"))
	if cd == iconvBad() {
		t.Fatal("iconv_open(GBK,UTF-8) failed")
	}
	defer IconvClose(cd)

	in := esc([]byte{0xe4, 0xbd}) // first 2 of the 3 UTF-8 bytes of 你 (E4 BD A0)
	out := esc(make([]byte, 16))
	inp, op := &in[0], &out[0]
	base := uintptr(unsafe.Pointer(&in[0]))
	inleft, outleft := uint64(len(in)), uint64(len(out))
	*ErrnoPtr() = 0

	r := Iconv(cd, &inp, &inleft, &op, &outleft)
	if r != iconvErr {
		t.Fatalf("iconv (split multibyte) = %d, want -1", r)
	}
	if e := *ErrnoPtr(); e != errEINVAL {
		t.Fatalf("errno = %d, want EINVAL(%d)", e, errEINVAL)
	}
	if got := uintptr(unsafe.Pointer(inp)); got != base {
		t.Fatalf("inbuf advanced to %#x, want %#x (incomplete lead byte preserved)", got, base)
	}
	if inleft != uint64(len(in)) {
		t.Fatalf("inleft = %d, want %d (incomplete tail preserved)", inleft, len(in))
	}
}

// TestIconvEILSEQ: a source rune the target encoding cannot represent yields
// (size_t)-1 + EILSEQ. (x/text decoders are lenient, so EILSEQ comes from the
// encode side — see iconv.go.)
func TestIconvEILSEQ(t *testing.T) {
	cd := IconvOpen(cstrOf("ISO-8859-1"), cstrOf("UTF-8"))
	if cd == iconvBad() {
		t.Fatal("iconv_open(ISO-8859-1,UTF-8) failed")
	}
	defer IconvClose(cd)

	// '€' (U+20AC) has no ISO-8859-1 representation; 'A' before / 'B' after make
	// it a complete, mid-stream, unmappable rune.
	src := []byte("A€B")
	in := esc(append([]byte(nil), src...))
	out := esc(make([]byte, 16))
	inp, op := &in[0], &out[0]
	inleft, outleft := uint64(len(in)), uint64(len(out))
	*ErrnoPtr() = 0

	r := Iconv(cd, &inp, &inleft, &op, &outleft)
	// musl (#658 M5): an unmappable rune toward the TARGET is substituted with
	// '*' and COUNTED — not an error (EILSEQ is for malformed SOURCE bytes).
	if r != 1 {
		t.Fatalf("iconv (unmappable) = %d, want 1 substitution", r)
	}
	if inleft != 0 {
		t.Fatalf("inleft = %d, want 0 (conversion completed)", inleft)
	}
	if got := string(out[:len(out)-int(outleft)]); got != "A*B" {
		t.Fatalf("output = %q, want %q", got, "A*B")
	}
}

// TestIconvOpenBogus: an unknown codeset yields (iconv_t)-1 + EINVAL.
func TestIconvOpenBogus(t *testing.T) {
	*ErrnoPtr() = 0
	cd := IconvOpen(cstrOf("NO-SUCH-ENCODING-XYZ"), cstrOf("UTF-8"))
	if cd != iconvBad() {
		t.Fatalf("iconv_open(bogus) = %v, want (iconv_t)-1", cd)
	}
	if e := *ErrnoPtr(); e != errEINVAL {
		t.Fatalf("errno = %d, want EINVAL(%d)", e, errEINVAL)
	}
}

// TestIconvOpenUnsupportedPair: a conversion with NEITHER side UTF-8 (GBK→Big5)
// is rejected at open with (iconv_t)-1 + EINVAL — an honest, POSIX-permitted
// refusal rather than a Chain that reports an unfaithful source cursor.
func TestIconvOpenUnsupportedPair(t *testing.T) {
	*ErrnoPtr() = 0
	cd := IconvOpen(cstrOf("BIG5"), cstrOf("GBK"))
	if cd != iconvBad() {
		t.Fatalf("iconv_open(BIG5,GBK) = %v, want (iconv_t)-1 (unsupported pair)", cd)
	}
	if e := *ErrnoPtr(); e != errEINVAL {
		t.Fatalf("errno = %d, want EINVAL(%d)", e, errEINVAL)
	}
}

// TestIconvReset: the reset/flush call (inbuf==NULL) returns 0 and IconvClose
// returns 0.
func TestIconvReset(t *testing.T) {
	cd := IconvOpen(cstrOf("GBK"), cstrOf("UTF-8"))
	if cd == iconvBad() {
		t.Fatal("iconv_open failed")
	}
	if r := Iconv(cd, nil, nil, nil, nil); r != 0 {
		t.Fatalf("iconv reset = %d, want 0", r)
	}
	if rc := IconvClose(cd); rc != 0 {
		t.Fatalf("iconv_close = %d, want 0", rc)
	}
}

// TestIconvStrictSource (#658 M5): malformed SOURCE bytes are EILSEQ with the
// cursor parked on the offending byte — in all three directions — while a
// GENUINE U+FFFD from a Unicode-complete source passes through.
func TestIconvStrictSource(t *testing.T) {
	run := func(to, from string, src []byte, outCap int) (uint64, uint64, uint64, int32, []byte) {
		t.Helper()
		cd := IconvOpen(cstrOf(to), cstrOf(from))
		if cd == iconvBad() {
			t.Fatalf("iconv_open(%s,%s) failed", to, from)
		}
		defer IconvClose(cd)
		in := esc(append([]byte(nil), src...))
		out := esc(make([]byte, outCap))
		inp, op := &in[0], &out[0]
		inleft, outleft := uint64(len(in)), uint64(len(out))
		*ErrnoPtr() = 0
		r := Iconv(cd, &inp, &inleft, &op, &outleft)
		return r, inleft, outleft, *ErrnoPtr(), out[:uint64(outCap)-outleft]
	}

	// UTF-8 -> UTF-8 (validated copy): 0xFF after "AB".
	r, inleft, _, e, got := run("UTF-8", "UTF-8", []byte{'A', 'B', 0xFF, 'C'}, 32)
	if r != iconvErr || e != errEILSEQ {
		t.Fatalf("copy: r=%d errno=%d, want -1/EILSEQ", r, e)
	}
	if inleft != 2 || string(got) != "AB" {
		t.Errorf("copy: inleft=%d out=%q, want 2 %q (cursor on the bad byte)", inleft, got, "AB")
	}

	// UTF-8 -> ISO-8859-1 (encode): a malformed two-byte sequence C3 28.
	r, inleft, _, e, got = run("ISO-8859-1", "UTF-8", []byte{'A', 0xC3, 0x28, 'B'}, 32)
	if r != iconvErr || e != errEILSEQ {
		t.Fatalf("encode: r=%d errno=%d, want -1/EILSEQ", r, e)
	}
	if inleft != 3 || string(got) != "A" {
		t.Errorf("encode: inleft=%d out=%q, want 3 %q", inleft, got, "A")
	}

	// UTF-8 -> ISO-8859-1: an INCOMPLETE trailing sequence is EINVAL, not EILSEQ.
	r, inleft, _, e, _ = run("ISO-8859-1", "UTF-8", []byte{'A', 0xC3}, 32)
	if r != iconvErr || e != errEINVAL || inleft != 1 {
		t.Errorf("incomplete: r=%d errno=%d inleft=%d, want -1/EINVAL/1", r, e, inleft)
	}

	// GBK -> UTF-8 (decode; GBK cannot express U+FFFD): invalid trail byte.
	r, inleft, _, e, got = run("UTF-8", "GBK", []byte{'A', 0x81, 0x20, 'B'}, 32)
	if r != iconvErr || e != errEILSEQ {
		t.Fatalf("decode: r=%d errno=%d, want -1/EILSEQ", r, e)
	}
	if inleft != 3 || string(got) != "A" {
		t.Errorf("decode: inleft=%d out=%q, want 3 %q", inleft, got, "A")
	}

	// A genuine U+FFFD survives a Unicode-complete source (UTF-16LE: FD FF).
	r, inleft, _, e, got = run("UTF-8", "UTF-16LE", []byte{'A', 0x00, 0xFD, 0xFF, 'B', 0x00}, 32)
	if r != 0 || e != 0 || inleft != 0 {
		t.Fatalf("genuine FFFD: r=%d errno=%d inleft=%d, want 0/0/0", r, e, inleft)
	}
	if string(got) != "A�B" {
		t.Errorf("genuine FFFD out = %q, want %q", got, "A�B")
	}
}
