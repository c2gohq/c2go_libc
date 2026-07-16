// iconv.go — POSIX codeset conversion (<iconv.h>), a Go bridge over
// golang.org/x/text/encoding. Unlike the C-ported multibyte/wstring code, iconv
// exists here for x/text's *encoding tables*, so it is implemented in Go and
// exported to C via //go:linkname (same shape as sem.go): the C header declares
// each prototype with a c2go_linkname pointing at the PascalCase Go symbol,
// reached across the GoABI0 boundary.
//
// A conversion uses a SINGLE x/text transformer so the reported source cursor
// (nSrc) honors POSIX's contract — it counts only source bytes whose output has
// fully landed in the caller's (bounded) output buffer. A two-stage
// transform.Chain would eagerly drain all source into an internal buffer,
// reporting nSrc = the whole input while only one output-window's worth was
// emitted → silent truncation of a bounded-output loop. A single transformer is
// possible whenever one side is UTF-8 (encoding.Nop): X→UTF-8 is the decoder
// alone, UTF-8→X is the encoder alone, UTF-8→UTF-8 is identity. A pair with
// NEITHER side UTF-8 (e.g. GBK→Big5) would need a faithful two-stage pump;
// iconv_open rejects it with EINVAL (POSIX-permitted for an unsupported pair —
// convert via UTF-8 in two steps). The iconv_t descriptor is an opaque handle
// value from iconvTab (handle.go); it holds no Go pointer, so the C side sees a
// plain void*.
//
// Strictness (#658 M5, musl semantics): x/text decoders are lenient by design
// (malformed source bytes become U+FFFD, never an error), so EILSEQ is
// reconstructed per direction: a UTF-8 SOURCE (copy/encode) is pre-scanned and
// only its valid prefix fed onward; a foreign source (decode) uses the FFFD
// SENTINEL — a U+FFFD in the output is either a replacement (EILSEQ) or a
// genuine U+FFFD, decided by comparing the source bytes against the charset's
// own encoding of U+FFFD (precomputed at open; charsets that cannot encode it
// make every FFFD an error). The error path replays a FRESH decoder to locate
// the offending byte, so the hot path pays one bytes.Index per window. An
// unrepresentable rune toward the TARGET follows musl: substitute '*' and
// count it in the return value (not an error). //TRANSLIT and //IGNORE
// suffixes are parsed off the codeset name but their behavior is deferred.

package libc

import (
	"bytes"
	"strings"
	"sync/atomic"
	"unicode/utf8"
	"unsafe"

	"golang.org/x/text/encoding"
	"golang.org/x/text/encoding/charmap"
	"golang.org/x/text/encoding/ianaindex"
	"golang.org/x/text/encoding/japanese"
	"golang.org/x/text/encoding/korean"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/encoding/traditionalchinese"
	"golang.org/x/text/encoding/unicode"
	"golang.org/x/text/encoding/unicode/utf32"
	"golang.org/x/text/transform"
)

// iconvState holds the from->to transformer for one open descriptor. The
// descriptor handed to C is the REAL *iconvState pointer (a Go heap object,
// GC-legal in any pointer slot); iconvTab exists purely to ROOT it for the
// C-held lifetime (id records its slot for iconv_close's free).
type iconvState struct {
	t   transform.Transformer
	dir int8                         // 0 = UTF-8 copy, 1 = decode (X->UTF-8), 2 = encode (UTF-8->X)
	mk  func() transform.Transformer // fresh-decoder factory: EILSEQ localization replay (dir 1)
	fffd []byte                      // source-charset encoding of U+FFFD, nil if inexpressible (dir 1)
	id  uint64
}

var iconvTab handleTable[iconvState]

// resolveEncoding maps an iconv codeset name to an x/text Encoding. It first
// consults a hand-written alias table for the common iconv spellings (which
// ianaindex misses or spells differently), then falls back to ianaindex.MIME /
// ianaindex.IANA. ianaindex.Encoding can return (nil, nil) for a recognized but
// UNSUPPORTED charset, so a nil encoding is treated as failure.
func resolveEncoding(name string) (encoding.Encoding, bool) {
	n := strings.TrimSpace(name)
	// Drop a trailing //TRANSLIT or //IGNORE suffix (behavior deferred).
	if i := strings.Index(n, "//"); i >= 0 {
		n = n[:i]
	}
	n = strings.ToUpper(strings.TrimSpace(n))

	switch n {
	case "", "UTF-8", "UTF8":
		return encoding.Nop, true
	case "UTF-16LE", "UTF16LE", "UCS-2LE":
		return unicode.UTF16(unicode.LittleEndian, unicode.IgnoreBOM), true
	case "UTF-16BE", "UTF16BE", "UCS-2BE":
		return unicode.UTF16(unicode.BigEndian, unicode.IgnoreBOM), true
	case "UTF-16", "UTF16":
		return unicode.UTF16(unicode.BigEndian, unicode.UseBOM), true
	case "UTF-32LE", "UTF32LE", "UCS-4LE":
		return utf32.UTF32(utf32.LittleEndian, utf32.IgnoreBOM), true
	case "UTF-32BE", "UTF32BE", "UCS-4BE":
		return utf32.UTF32(utf32.BigEndian, utf32.IgnoreBOM), true
	case "UTF-32", "UTF32", "UCS-4":
		return utf32.UTF32(utf32.BigEndian, utf32.UseBOM), true
	case "GBK", "CP936", "GB2312", "EUC-CN", "EUCCN":
		return simplifiedchinese.GBK, true
	case "GB18030":
		return simplifiedchinese.GB18030, true
	case "BIG5", "BIG-5", "CP950":
		return traditionalchinese.Big5, true
	case "EUC-JP", "EUCJP":
		return japanese.EUCJP, true
	case "SHIFT_JIS", "SHIFT-JIS", "SJIS", "CP932":
		return japanese.ShiftJIS, true
	case "EUC-KR", "EUCKR", "CP949", "UHC":
		return korean.EUCKR, true
	case "ISO-8859-1", "ISO8859-1", "LATIN1", "L1", "CP819":
		return charmap.ISO8859_1, true
	case "WINDOWS-1252", "CP1252":
		return charmap.Windows1252, true
	case "ASCII", "ANSI_X3.4-1968":
		n = "US-ASCII" // ianaindex knows US-ASCII (the real x/text ASCII), not the bare spellings
	}
	// Fall back to ianaindex (US-ASCII and the long tail resolve here).
	if e, err := ianaindex.MIME.Encoding(n); err == nil && e != nil {
		return e, true
	}
	if e, err := ianaindex.IANA.Encoding(n); err == nil && e != nil {
		return e, true
	}
	return nil, false
}

// IconvOpen returns the REAL *iconvState pointer, or nil on failure. The
// POSIX (iconv_t)-1 failure sentinel exists ONLY in the C world: the C
// wrapper (source/iconv.c) maps nil→-1 and screens -1 back out — a
// manufactured non-pointer value must never live in a Go pointer slot (a
// prior integer-handle design parked id 1 / ^0 in unsafe.Pointer stack slots
// and the precise stack scan threw "invalid pointer found on stack").
//
//go:linkname IconvOpen
func IconvOpen(to, from *byte) unsafe.Pointer {
	toEnc, ok1 := resolveEncoding(cstr(to))
	fromEnc, ok2 := resolveEncoding(cstr(from))
	if !ok1 || !ok2 {
		setErrno(errEINVAL)
		return nil // C wrapper maps nil -> (iconv_t)-1
	}
	// Build a SINGLE transformer (see file header): a faithful nSrc is only
	// possible when one side is UTF-8 (encoding.Nop). The encoder/decoder's built
	// in UTF-8 reader also reports an incomplete trailing multibyte sequence as
	// ErrShortSrc (→ EINVAL) under atEOF=false, keeping those bytes in inbuf.
	fromNop := fromEnc == encoding.Nop
	toNop := toEnc == encoding.Nop
	st := &iconvState{}
	switch {
	case fromNop && toNop:
		// UTF-8 → UTF-8: a validating pass-through (#658 M5) — plain Nop let
		// malformed bytes tunnel through unreported.
		st.t = encoding.UTF8Validator
		st.dir = 0
	case toNop:
		st.t = fromEnc.NewDecoder() // X → UTF-8
		st.dir = 1
		st.mk = func() transform.Transformer { return fromEnc.NewDecoder() }
		if b, err := fromEnc.NewEncoder().Bytes([]byte("\uFFFD")); err == nil {
			st.fffd = b // the charset CAN express U+FFFD: sentinel needs the byte compare
		}
	case fromNop:
		st.t = toEnc.NewEncoder() // UTF-8 → X
		st.dir = 2
	default:
		// Neither side is UTF-8 (e.g. GBK→Big5): a single transformer can't do it
		// and Chain can't report a faithful nSrc. POSIX lets iconv_open reject an
		// unsupported conversion with EINVAL; pivot through UTF-8 in two steps.
		setErrno(errEINVAL)
		return nil // C wrapper maps nil -> (iconv_t)-1
	}
	st.id = iconvTab.alloc(st) // root while C holds the pointer
	return unsafe.Pointer(st)
}

//go:linkname Iconv
func Iconv(cd unsafe.Pointer, inbuf **byte, inleft *uint64, outbuf **byte, outleft *uint64) uint64 {
	if cd == nil {
		setErrno(errEINVAL) // the C wrapper screens (iconv_t)-1; nil is defensive
		return ^uint64(0)
	}
	st := (*iconvState)(cd)
	// Reset/flush call: iconv(cd, NULL, ...) or *inbuf==NULL. These encodings
	// carry no output shift state, so Reset() is enough (no bytes emitted).
	if inbuf == nil || *inbuf == nil {
		st.t.Reset()
		return 0
	}
	in := unsafe.Slice(*inbuf, *inleft)
	out := unsafe.Slice(*outbuf, *outleft)

	nDst, nSrc, subs, errno := iconvRun(st, in, out)

	// Advance the C cursors by what was consumed/produced (POSIX contract).
	*inbuf = (*byte)(unsafe.Add(unsafe.Pointer(*inbuf), nSrc))
	*inleft -= uint64(nSrc)
	*outbuf = (*byte)(unsafe.Add(unsafe.Pointer(*outbuf), nDst))
	*outleft -= uint64(nDst)

	if errno != 0 {
		setErrno(errno)
		return ^uint64(0)
	}
	return subs // musl: the count of '*'-substituted (nonreversible) conversions
}

// fffdUTF8 is U+FFFD's UTF-8 form — what a lenient x/text decoder emits for a
// malformed source byte (the strictness sentinel, #658 M5).
var fffdUTF8 = []byte{0xEF, 0xBF, 0xBD}

// utf8ValidPrefix scans b and returns the length of its longest valid UTF-8
// prefix, plus whether the byte that stopped the scan begins an INCOMPLETE
// (still completable) sequence rather than a malformed one.
func utf8ValidPrefix(b []byte) (n int, incomplete bool) {
	for n < len(b) {
		if b[n] < utf8.RuneSelf {
			n++
			continue
		}
		r, sz := utf8.DecodeRune(b[n:])
		if r == utf8.RuneError && sz <= 1 {
			return n, !utf8.FullRune(b[n:]) // truncated tail => need more input
		}
		n += sz
	}
	return n, false
}

// iconvRun performs one window's conversion with musl-strict semantics:
// (produced, consumed, substitutions, errno).
func iconvRun(st *iconvState, in, out []byte) (int, int, uint64, int32) {
	switch st.dir {
	case 0, 2:
		// UTF-8 source: pre-scan (the file header explains why a Chain'd
		// validator would break the nSrc contract) and feed the valid prefix.
		nvalid, incomplete := utf8ValidPrefix(in)
		nDst, nSrc, subs, errno := 0, 0, uint64(0), int32(0)
		if st.dir == 0 {
			// Validated copy — truncation must land on a character boundary
			// (POSIX: no partial character in the output).
			m := nvalid
			if m > len(out) {
				m = len(out)
				for m > 0 && in[m]&0xC0 == 0x80 {
					m--
				}
			}
			copy(out, in[:m])
			nDst, nSrc = m, m
			if m < nvalid {
				errno = errE2BIG
			}
		} else {
			for nSrc < nvalid {
				d, c, err := st.t.Transform(out[nDst:], in[nSrc:nvalid], false)
				nDst += d
				nSrc += c
				if err == nil {
					break
				}
				if err == transform.ErrShortDst {
					errno = errE2BIG
					break
				}
				if err == transform.ErrShortSrc {
					errno = errEINVAL // defensive: whole runes were fed
					break
				}
				// Repertoire error: the target cannot express this rune.
				// musl substitutes '*' and counts it (never EILSEQ here).
				if nDst >= len(out) {
					errno = errE2BIG
					break
				}
				out[nDst] = '*'
				nDst++
				_, sz := utf8.DecodeRune(in[nSrc:nvalid])
				nSrc += sz
				subs++
			}
		}
		if errno == 0 && nvalid < len(in) && nSrc == nvalid {
			if incomplete {
				errno = errEINVAL // partial sequence: wait for more input
			} else {
				errno = errEILSEQ // malformed byte, cursor parked on it
			}
		}
		return nDst, nSrc, subs, errno

	default: // dir 1: decode X -> UTF-8
		nDst, nSrc, err := st.t.Transform(out, in, false)
		// FFFD sentinel: every replacement the lenient decoder makes surfaces
		// as U+FFFD in the produced window; a genuine source U+FFFD is told
		// apart by its bytes (st.fffd; nil = charset can't express one).
		scan := 0
		for {
			j := bytes.Index(out[scan:nDst], fffdUTF8)
			if j < 0 {
				break
			}
			j += scan
			pre, ok := iconvLocate(st, in, out[:j])
			if ok && st.fffd != nil && len(in) >= pre+len(st.fffd) &&
				bytes.Equal(in[pre:pre+len(st.fffd)], st.fffd) {
				scan = j + len(fffdUTF8) // genuine U+FFFD: keep scanning after it
				continue
			}
			// Replacement => EILSEQ. Deliver only the prefix before it and park
			// the source cursor on the offending byte; the stateful transformer
			// restarts fresh from that byte.
			st.t = st.mk()
			if !ok {
				return 0, 0, 0, errEILSEQ // spanning/replay mismatch: consume nothing
			}
			return j, pre, 0, errEILSEQ
		}
		switch err {
		case nil:
			return nDst, nSrc, 0, 0
		case transform.ErrShortDst:
			return nDst, nSrc, 0, errE2BIG
		case transform.ErrShortSrc:
			return nDst, nSrc, 0, errEINVAL
		default:
			return nDst, nSrc, 0, errEILSEQ
		}
	}
}

// iconvLocate replays a FRESH decoder over in, capping output at len(prefix),
// and returns how many source bytes produce exactly that prefix. ok=false when
// the replay diverges (a sequence spanning iconv calls — the stateful
// transformer carried bytes from the previous window).
func iconvLocate(st *iconvState, in, prefix []byte) (int, bool) {
	f := st.mk()
	dst := make([]byte, len(prefix))
	nDst, nSrc := 0, 0
	for {
		d, c, err := f.Transform(dst[nDst:], in[nSrc:], false)
		nDst += d
		nSrc += c
		if nDst >= len(prefix) || err == nil ||
			(err != transform.ErrShortDst && err != transform.ErrShortSrc) {
			break
		}
		if d == 0 && c == 0 {
			break // no progress
		}
	}
	if nDst != len(prefix) || !bytes.Equal(dst[:nDst], prefix) {
		return 0, false
	}
	return nSrc, true
}


//go:linkname IconvClose
func IconvClose(cd unsafe.Pointer) int32 {
	if cd == nil {
		setErrno(errEBADF)
		return -1
	}
	// #658 M6: claim the id atomically so a double iconv_close fails honestly
	// (EBADF) instead of freeing a reused slot's occupant.
	id := atomic.SwapUint64(&(*iconvState)(cd).id, 0)
	if id == 0 {
		setErrno(errEBADF)
		return -1
	}
	iconvTab.free(id) // drop the root; the GC reclaims the state
	return 0
}
