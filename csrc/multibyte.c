/* multibyte.c — UTF-8 <-> wchar_t conversions, ported VERBATIM from musl's
 * src/multibyte. This is a faithful C port (NOT a Go bridge): mbstate_t
 * streaming and the incomplete(-2)/illegal(-1,EILSEQ) distinction cannot be
 * expressed by Go's unicode/utf8, and it is a per-char hot path the C-world
 * printf/scanf call directly. UNIX-FIRST: wchar_t is int32 (UTF-32); this libc
 * is always C.UTF-8, so __ctype_get_mb_cur_max() is a constant 4 and every
 * musl `MB_CUR_MAX==1` C-locale branch is dead — kept verbatim for byte-
 * faithfulness rather than deleted. On Windows wchar_t is uint16 (UTF-16): a
 * supplementary scalar (>= U+10000) is carried as a surrogate PAIR of two
 * wchar_t. The string converters emit/combine pairs; the single-char converters
 * cannot fit one in a wchar_t and report EILSEQ (see WCHAR_UTF16 below). All the
 * surrogate code is guarded by `sizeof(wchar_t) < 4`, a compile-time constant
 * that clang folds away on unix, so the UTF-32 path is byte-for-byte musl. Each
 * function is c2go_extern (the export authority); its <wchar.h>
 * or <stdlib.h> declaration carries the matching c2go_linkname + ABI0 CC. */
#include <stdlib.h>
#include <wchar.h>
#include <bits/wchar_impl.h>   /* WCHAR_UTF16 / SURR_* surrogate predicates */
#include <errno.h>
#include <stdio.h>   /* EOF (btowc/wctob) */
#include <stdint.h>  /* uint32_t/int32_t/uintptr_t for the state table + fast path */
#include <string.h>  /* strlen (mbsrtowcs C-locale branch), memcpy (wcsnrtombs) */
#include <limits.h>  /* MB_LEN_MAX (4: UTF-8). No #ifndef re-pin — a local
                      * fallback masked the windows-goabi TU where clang's
                      * resource <limits.h> defined it as 1 (buffer overflow). */
#include <c2go.h>

/* Inside this TU MB_CUR_MAX is the constant 4 (this libc is always C.UTF-8),
 * matching musl's inline locale_impl.h value. <stdlib.h>'s macro expands to a
 * __ctype_get_mb_cur_max() call, which on this per-character hot path would be a
 * real GoABI0 function call every non-ASCII char; the constant lets clang fold
 * the dead `MB_CUR_MAX==1` C-locale branches away. Byte behaviour is identical. */
#undef MB_CUR_MAX
#define MB_CUR_MAX 4

/* ── internal state machine (musl src/multibyte/internal.{h,c}) ──────────────
 * `hidden`, the locale_impl.h dependency and the __fsmu8 alias are dropped; the
 * bittab is file-local. A DFA state is a uint32: the low bits accumulate the
 * code point, the upper 6 bits are a negative offset used by OOB() to range-
 * check the next continuation byte. */

/* Upper 6 state bits are a negative integer offset to bound-check next byte */
/*    equivalent to: ( (b-0x80) | (b+offset) ) & ~0x3f      */
#define OOB(c,b) (((((b)>>3)-0x10)|(((b)>>3)+((int32_t)(c)>>26))) & ~7)

/* Interval [a,b). Either a must be 80 or b must be c0, lower 3 bits clear. */
#define R(a,b) ((uint32_t)((a==0x80 ? 0x40u-b : 0u-a) << 23))
#define FAILSTATE R(0x80,0x80)

#define SA 0xc2u
#define SB 0xf4u

/* Arbitrary encoding for representing code units instead of characters. */
#define CODEUNIT(c) (0xdfff & (signed char)(c))
#define IS_CODEUNIT(c) ((unsigned)(c)-0xdf80 < 0x80)

#define C(x) ( x<2 ? -1 : ( R(0x80,0xc0) | x ) )
#define D(x) C((x+16))
#define E(x) ( ( x==0 ? R(0xa0,0xc0) : \
                 x==0xd ? R(0x80,0xa0) : \
                 R(0x80,0xc0) ) \
             | ( R(0x80,0xc0) >> 6 ) \
             | x )
#define F(x) ( ( x>=5 ? 0 : \
                 x==0 ? R(0x90,0xc0) : \
                 x==4 ? R(0x80,0x90) : \
                 R(0x80,0xc0) ) \
             | ( R(0x80,0xc0) >> 6 ) \
             | ( R(0x80,0xc0) >> 12 ) \
             | x )

static const uint32_t bittab[] = {
	              C(0x2),C(0x3),C(0x4),C(0x5),C(0x6),C(0x7),
	C(0x8),C(0x9),C(0xa),C(0xb),C(0xc),C(0xd),C(0xe),C(0xf),
	D(0x0),D(0x1),D(0x2),D(0x3),D(0x4),D(0x5),D(0x6),D(0x7),
	D(0x8),D(0x9),D(0xa),D(0xb),D(0xc),D(0xd),D(0xe),D(0xf),
	E(0x0),E(0x1),E(0x2),E(0x3),E(0x4),E(0x5),E(0x6),E(0x7),
	E(0x8),E(0x9),E(0xa),E(0xb),E(0xc),E(0xd),E(0xe),E(0xf),
	F(0x0),F(0x1),F(0x2),F(0x3),F(0x4)
};

#undef C
#undef D
#undef E
#undef F

/* ── UTF-16 (Windows) surrogate support ─────────────────────────────────────
 * The WCHAR_UTF16 / SURR_* / IS_*_SURR / IS_4BYTE_LEAD predicates live in
 * <bits/wchar_impl.h> (shared with the wide FILE I/O in stdio.c). WCHAR_UTF16 is
 * a compile-time constant, so clang folds every surrogate branch away on the
 * unix UTF-32 targets, leaving the musl code byte-identical there.
 *
 * Combine a validated high+low surrogate pair into its 4-byte UTF-8 form. Used
 * on UTF-16 targets by the wide->byte string converters here AND by the
 * printf/scanf wide conversions in stdio.c (their per-unit wcrtomb/wctomb cannot
 * take a > U+FFFF scalar when wchar_t is 16 bits). Exported (c2go_extern + a
 * <bits/wchar_impl.h> decl) so stdio.c can reach it cross-TU. */
c2go_extern size_t __surrogate_to_utf8(char *s, unsigned hi, unsigned lo) {
	unsigned cp = 0x10000u + ((hi - 0xd800u) << 10) + (lo - 0xdc00u);
	s[0] = 0xf0 | (cp >> 18);
	s[1] = 0x80 | ((cp >> 12) & 0x3f);
	s[2] = 0x80 | ((cp >> 6) & 0x3f);
	s[3] = 0x80 | (cp & 0x3f);
	return 4;
}

/* ── MB_CUR_MAX (musl src/locale/__ctype_get_mb_cur_max.c) ──────────────────
 * musl returns the locale's value; this libc is always C.UTF-8, so it is the
 * constant 4. Returns a literal (NOT the MB_CUR_MAX macro, which expands to a
 * call to this very function). */
c2go_extern size_t __ctype_get_mb_cur_max(void) {
    return 4;
}

/* ── single character: byte(s) -> wide (musl mbrtowc.c / mbtowc.c) ───────────
 * Return contract: byte count on success, 0 for an embedded NUL, (size_t)-1 +
 * errno=EILSEQ for illegal, (size_t)-2 for an incomplete prefix. mbrtowc keeps
 * partial state in *st (or a static internal_state when st==NULL). */

/* __mbrtoc32 is musl mbrtowc's DFA verbatim, but it writes the full 32-bit
 * SCALAR to *pc (a char32_t) — it never surrogate-splits. It is the shared
 * byte->wide primitive: the public mbrtowc adds the wchar_t-width policy, and
 * the string engines use it to reach the scalar so they can emit surrogate
 * pairs on UTF-16 targets. (This is exactly C11 mbrtoc32.) Exported (c2go_extern
 * + a <wchar.h> decl) so the wide FILE I/O in stdio.c can reach the scalar too. */
/* internal core: KEEPCASE — a CamelCase Go alias would collide with the
 * public mbrtoc32's Mbrtoc32 (#673). The object-like c2go_extern macro
 * shadows the attribute spelling, so the arg form needs push_macro (#648). */
#pragma push_macro("c2go_extern")
#undef c2go_extern
__attribute__((c2go_extern(C2GO_KEEPCASE)))
size_t __mbrtoc32(unsigned *restrict pc, const char *restrict src, size_t n, mbstate_t *restrict st) {
	static unsigned internal_state;
	unsigned c;
	const unsigned char *s = (const void *)src;
	const size_t N = n;
	unsigned dummy;

	if (!st) st = (void *)&internal_state;
	c = *(unsigned *)st;

	if (!s) {
		if (c) goto ilseq;
		return 0;
	} else if (!pc) pc = &dummy;

	if (!n) return -2;
	if (!c) {
		if (*s < 0x80) return !!(*pc = *s);
		if (MB_CUR_MAX==1) return (*pc = CODEUNIT(*s)), 1;
		if (*s-SA > SB-SA) goto ilseq;
		c = bittab[*s++-SA]; n--;
	}

	if (n) {
		if (OOB(c,*s)) goto ilseq;
loop:
		c = c<<6 | *s++-0x80; n--;
		if (!(c&(1U<<31))) {
			*(unsigned *)st = 0;
			*pc = c;
			return N-n;
		}
		if (n) {
			if (*s-0x80u >= 0x40) goto ilseq;
			goto loop;
		}
	}

	*(unsigned *)st = c;
	return -2;
ilseq:
	*(unsigned *)st = 0;
	errno = EILSEQ;
	return -1;
}
#pragma pop_macro("c2go_extern")

c2go_extern size_t mbrtowc(wchar_t *restrict wc, const char *restrict src, size_t n, mbstate_t *restrict st) {
	unsigned cp;
	size_t r = __mbrtoc32(&cp, src, n, st);
	/* r is a byte count (0..4) on success; the incomplete/illegal returns are
	 * the huge (size_t)-2/-1. Only a real decode (src!=NULL) writes the result.
	 * A single wchar_t cannot hold a supplementary scalar on UTF-16 (Windows) —
	 * report EILSEQ; supplementary text must go through the string converters. */
	if (r <= 4 && src) {
		if (WCHAR_UTF16 && cp >= 0x10000) { errno = EILSEQ; return -1; }
		if (wc) *wc = cp;
	}
	return r;
}

c2go_extern int mbtowc(wchar_t *restrict wc, const char *restrict src, size_t n) {
	unsigned c;
	const unsigned char *s = (const void *)src;
	wchar_t dummy;

	if (!s) return 0;
	if (!n) goto ilseq;
	if (!wc) wc = &dummy;

	if (*s < 0x80) return !!(*wc = *s);
	if (MB_CUR_MAX==1) return (*wc = CODEUNIT(*s)), 1;
	if (*s-SA > SB-SA) goto ilseq;
	c = bittab[*s++-SA];

	/* Avoid excessive checks against n: If shifting the state n-1
	 * times does not clear the high bit, then the value of n is
	 * insufficient to read a character */
	if (n<4 && ((c<<(6*n-6)) & (1U<<31))) goto ilseq;

	if (OOB(c,*s)) goto ilseq;
	c = c<<6 | *s++-0x80;
	if (!(c&(1U<<31))) {
		*wc = c;
		return 2;
	}

	if (*s-0x80u >= 0x40) goto ilseq;
	c = c<<6 | *s++-0x80;
	if (!(c&(1U<<31))) {
		*wc = c;
		return 3;
	}

	if (*s-0x80u >= 0x40) goto ilseq;
	/* A 4-byte sequence is a supplementary scalar (>= U+10000); on UTF-16
	 * (Windows) it does not fit one wchar_t — non-restartable mbtowc cannot
	 * split it, so report EILSEQ (folds away on unix). */
	if (WCHAR_UTF16) goto ilseq;
	*wc = c<<6 | *s++-0x80;
	return 4;

ilseq:
	errno = EILSEQ;
	return -1;
}

/* ── single character: wide -> byte(s) (musl wcrtomb.c / wctomb.c) ───────────
 * Return contract: byte count on success, (size_t)-1 + errno=EILSEQ for a wide
 * value with no UTF-8 encoding (a surrogate or > U+10FFFF).
 *
 * __c32rtomb is the musl wcrtomb body verbatim with a 32-bit SCALAR param —
 * the encode mirror of __mbrtoc32 (#673): on the UTF-16 windows target a
 * wchar_t cannot carry >0xFFFF, so uchar.c's c16rtomb/c32rtomb must reach the
 * scalar encoder directly. The public wcrtomb below is its wchar_t-width
 * policy wrapper ((unsigned)wc: zero-extend on the 16-bit target, the body's
 * own cast on the 32-bit ones — byte behaviour identical to the old direct
 * body). Exported (c2go_extern + a <wchar.h> decl) like __mbrtoc32. */

/* internal core: KEEPCASE — a CamelCase Go alias would collide with the
 * public c32rtomb's C32rtomb (#673); push_macro per #648. */
#pragma push_macro("c2go_extern")
#undef c2go_extern
__attribute__((c2go_extern(C2GO_KEEPCASE)))
size_t __c32rtomb(char *restrict s, unsigned wc, mbstate_t *restrict st) {
	if (!s) return 1;
	if ((unsigned)wc < 0x80) {
		*s = wc;
		return 1;
	} else if (MB_CUR_MAX == 1) {
		if (!IS_CODEUNIT(wc)) {
			errno = EILSEQ;
			return -1;
		}
		*s = wc;
		return 1;
	} else if ((unsigned)wc < 0x800) {
		*s++ = 0xc0 | (wc>>6);
		*s = 0x80 | (wc&0x3f);
		return 2;
	} else if ((unsigned)wc < 0xd800 || (unsigned)wc-0xe000 < 0x2000) {
		*s++ = 0xe0 | (wc>>12);
		*s++ = 0x80 | ((wc>>6)&0x3f);
		*s = 0x80 | (wc&0x3f);
		return 3;
	} else if ((unsigned)wc-0x10000 < 0x100000) {
		*s++ = 0xf0 | (wc>>18);
		*s++ = 0x80 | ((wc>>12)&0x3f);
		*s++ = 0x80 | ((wc>>6)&0x3f);
		*s = 0x80 | (wc&0x3f);
		return 4;
	}
	errno = EILSEQ;
	return -1;
}
#pragma pop_macro("c2go_extern")

c2go_extern size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict st) {
	return __c32rtomb(s, (unsigned)wc, st);
}

/* ── whole string: bytes -> wide (musl mbstowcs.c / mbsrtowcs.c) ──────────────
 * mbsrtowcs is the restartable engine; mbstowcs is the non-restartable façade.
 * The __GNUC__ word-at-a-time ASCII fast path is compiled (clang defines
 * __GNUC__) and reads 4 aligned bytes at a time, guarded by ASCII+alignment. */

c2go_extern size_t mbsrtowcs(wchar_t *restrict ws, const char **restrict src, size_t wn, mbstate_t *restrict st) {
	const unsigned char *s = (const void *)*src;
	size_t wn0 = wn;
	unsigned c = 0;
	unsigned resume_state = 0;   /* UTF-16: DFA state to restore if a resumed pair won't fit */

	if (st && (c = *(unsigned *)st)) {
		if (ws) {
			resume_state = c;
			*(unsigned *)st = 0;
			goto resume;
		} else {
			goto resume0;
		}
	}

	if (MB_CUR_MAX==1) {
		if (!ws) return strlen((const char *)s);
		for (;;) {
			if (!wn) {
				*src = (const void *)s;
				return wn0;
			}
			if (!*s) break;
			c = *s++;
			*ws++ = CODEUNIT(c);
			wn--;
		}
		*ws = 0;
		*src = 0;
		return wn0-wn;
	}

	if (!ws) for (;;) {
#ifdef __GNUC__
		typedef uint32_t __attribute__((__may_alias__)) w32;
		if (*s-1u < 0x7f && (uintptr_t)s%4 == 0) {
			while (!(( *(w32*)s | *(w32*)s-0x01010101) & 0x80808080)) {
				s += 4;
				wn -= 4;
			}
		}
#endif
		if (*s-1u < 0x7f) {
			s++;
			wn--;
			continue;
		}
		if (*s-SA > SB-SA) break;
		c = bittab[*s++-SA];
resume0:
		if (OOB(c,*s)) { s--; break; }
		s++;
		if (c&(1U<<25)) {
			if (*s-0x80u >= 0x40) { s-=2; break; }
			s++;
			if (c&(1U<<19)) {
				if (*s-0x80u >= 0x40) { s-=3; break; }
				s++;
				if (WCHAR_UTF16) wn--;   /* 4-byte char = supplementary = 2 UTF-16 units */
			}
		}
		wn--;
		c = 0;
	} else for (;;) {
		if (!wn) {
			*src = (const void *)s;
			return wn0;
		}
#ifdef __GNUC__
		typedef uint32_t __attribute__((__may_alias__)) w32;
		if (*s-1u < 0x7f && (uintptr_t)s%4 == 0) {
			while (wn>=5 && !(( *(w32*)s | *(w32*)s-0x01010101) & 0x80808080)) {
				*ws++ = *s++;
				*ws++ = *s++;
				*ws++ = *s++;
				*ws++ = *s++;
				wn -= 4;
			}
		}
#endif
		if (*s-1u < 0x7f) {
			*ws++ = *s++;
			wn--;
			continue;
		}
		if (*s-SA > SB-SA) break;
		/* On UTF-16 a supplementary scalar (a 4-byte sequence, lead 0xf0..0xf4)
		 * needs a 2-unit surrogate pair; if only one output slot remains, stop
		 * before consuming so the caller resumes with room (restartable). */
		if (WCHAR_UTF16 && wn < 2 && IS_4BYTE_LEAD(*s)) {
			*src = (const void *)s;
			return wn0 - wn;
		}
		c = bittab[*s++-SA];
resume:
		if (OOB(c,*s)) { s--; break; }
		c = (c<<6) | *s++-0x80;
		if (c&(1U<<31)) {
			if (*s-0x80u >= 0x40) { s-=2; break; }
			c = (c<<6) | *s++-0x80;
			if (c&(1U<<31)) {
				if (*s-0x80u >= 0x40) { s-=3; break; }
				c = (c<<6) | *s++-0x80;
			}
		}
		if (WCHAR_UTF16 && c >= 0x10000) {
			if (wn < 2) {
				/* reached only via resume (fresh pairs are peeked above): no room
				 * for the pair — restore the pre-resume DFA state and stop; *src
				 * is still at this call's start, so the next call re-resumes. */
				*(unsigned *)st = resume_state;
				return wn0 - wn;
			}
			*ws++ = SURR_HIGH(c);
			*ws++ = SURR_LOW(c);
			wn -= 2;
		} else {
			*ws++ = c;
			wn--;
		}
		c = 0;
	}

	if (!c && !*s) {
		if (ws) {
			*ws = 0;
			*src = 0;
		}
		return wn0-wn;
	}
	errno = EILSEQ;
	if (ws) *src = (const void *)s;
	return -1;
}

/* ── whole string: wide -> bytes (musl wcstombs.c / wcsrtombs.c) ─────────────
 * wcsrtombs is the restartable engine (it never leaves partial state — a wide
 * char always fits its 1-4 output bytes atomically); wcstombs is the façade. */

c2go_extern size_t wcsrtombs(char *restrict s, const wchar_t **restrict ws, size_t n, mbstate_t *restrict st) {
	const wchar_t *ws2;
	char buf[4];
	size_t N = n, l;
	/* On UTF-16 a supplementary scalar is a high+low surrogate PAIR (2 wchar_t)
	 * that wcrtomb — taking one 16-bit wchar_t — cannot encode; combine the pair
	 * into its 4 UTF-8 bytes directly. A lone/unpaired surrogate falls through to
	 * wcrtomb, which rejects it as EILSEQ. All pair branches fold away on unix. */
	if (!s) {
		for (n=0, ws2=*ws; *ws2; ws2++) {
			if (WCHAR_UTF16 && IS_HIGH_SURR(*ws2) && IS_LOW_SURR(ws2[1])) {
				n += 4;
				ws2++;               /* consume the low half too */
			} else if (*ws2 >= 0x80u) {
				l = wcrtomb(buf, *ws2, 0);
				if (!(l+1)) return -1;
				n += l;
			} else n++;
		}
		return n;
	}
	while (n>=4) {
		if (WCHAR_UTF16 && IS_HIGH_SURR(**ws) && IS_LOW_SURR((*ws)[1])) {
			__surrogate_to_utf8(s, **ws, (*ws)[1]);
			s += 4; n -= 4; (*ws) += 2;
			continue;
		}
		if (**ws-1u >= 0x7fu) {
			if (!**ws) {
				*s = 0;
				*ws = 0;
				return N-n;
			}
			l = wcrtomb(s, **ws, 0);
			if (!(l+1)) return -1;
			s += l;
			n -= l;
		} else {
			*s++ = **ws;
			n--;
		}
		(*ws)++;
	}
	while (n) {
		if (WCHAR_UTF16 && IS_HIGH_SURR(**ws) && IS_LOW_SURR((*ws)[1])) {
			if (4 > n) return N-n;   /* pair needs 4 bytes; not enough room yet */
			__surrogate_to_utf8(s, **ws, (*ws)[1]);
			s += 4; n -= 4; (*ws) += 2;
			continue;
		}
		if (**ws-1u >= 0x7fu) {
			if (!**ws) {
				*s = 0;
				*ws = 0;
				return N-n;
			}
			l = wcrtomb(buf, **ws, 0);
			if (!(l+1)) return -1;
			if (l>n) return N-n;
			wcrtomb(s, **ws, 0);
			s += l;
			n -= l;
		} else {
			*s++ = **ws;
			n--;
		}
		(*ws)++;
	}
	return N;
}

/* ── state helper (musl mbsinit.c) ─────────────────────────────────────────
 * mbsinit tests the initial (zero-accumulator) state. (mbrlen/mblen/wctomb/
 * mbstowcs/wcstombs — thin verbatim-musl wrappers over the engines here — are
 * built from the musl fork.) */

c2go_extern int mbsinit(const mbstate_t *st) {
	/* Initial iff both the UTF-8 accumulator (word 0) and the parked-low-
	 * surrogate slot (word 1, UTF-16 only) are clear. */
	return !st || (!((unsigned *)st)[0] && !((unsigned *)st)[1]);
}

/* ── bounded restartable string conversions (musl mbsnrtowcs.c / wcsnrtombs.c)
 * The n-bounded twins of mbsrtowcs/wcsrtombs — they cap the INPUT bytes/wide
 * chars consumed as well as the output count. mbsnrtowcs uses mbsrtowcs while
 * a whole batch is guaranteed to fit, then falls back to per-char mbrtowc. */

c2go_extern size_t mbsnrtowcs(wchar_t *restrict wcs, const char **restrict src, size_t n, size_t wn, mbstate_t *restrict st) {
	static mbstate_t internal_state;   /* 2 words: word1 UTF-8 partial, word2 UTF-16 parked-low */
	size_t l, cnt=0, n2;
	unsigned cp;
	wchar_t *ws, wbuf[256];
	const char *s = *src;
	const char *tmp_s;

	if (!st) st = &internal_state;
	if (!wcs) ws = wbuf, wn = sizeof wbuf / sizeof *wbuf;
	else ws = wcs;

	/* UTF-16 (Windows): a low surrogate parked by a prior call (whose output had
	 * room for only the high half of a supplementary pair) is emitted/counted
	 * first. For a counting call (!wcs) just tally and clear it — writing into
	 * wbuf would break the `ws != wbuf` sentinel below. */
	if (WCHAR_UTF16 && ((unsigned *)st)[1]) {
		if (wcs) {
			if (!wn) { *src = s; return 0; }   /* no room yet; stays parked */
			*ws++ = ((unsigned *)st)[1];
			wn--;
		}
		((unsigned *)st)[1] = 0;
		cnt++;
	}

	/* making sure output buffer size is at most n/4 will ensure
	 * that mbsrtowcs never reads more than n input bytes. thus
	 * we can use mbsrtowcs as long as it's practical.. */

	while ( s && wn && ( (n2=n/4)>=wn || n2>32 ) ) {
		if (n2>=wn) n2=wn;
		tmp_s = s;
		l = mbsrtowcs(ws, &s, n2, st);
		if (!(l+1)) {
			cnt = l;
			wn = 0;
			break;
		}
		/* mbsrtowcs made no progress: a supplementary pair does not fit this
		 * batch's output (n2 < 2). Fall to the per-char tail, which reserves the
		 * two slots or stops cleanly. Prevents an infinite bulk loop. */
		if (l == 0 && s == tmp_s) break;
		if (ws != wbuf) {
			ws += l;
			wn -= l;
		}
		n = s ? n - (s - tmp_s) : 0;
		cnt += l;
	}
	if (s) while (wn && n) {
		/* UTF-16: a fresh supplementary char (4-byte, lead 0xf0..0xf4) needs a
		 * 2-unit pair; stop before consuming if only one slot remains. */
		if (WCHAR_UTF16 && wn < 2 && IS_4BYTE_LEAD(*s)) break;
		/* Decode to the 32-bit scalar (not mbrtowc, which reports EILSEQ for a
		 * supplementary scalar on UTF-16 — here we split it into a pair). */
		l = __mbrtoc32(&cp, s, n, st);
		if (l+2<=2) {
			if (!(l+1)) {
				cnt = l;
				break;
			}
			if (!l) {
				*ws = 0;
				s = 0;
				break;
			}
			s += n;
			n -= n;
			break;
		}
		s += l; n -= l;
		/* safe - this loop runs fewer than sizeof(wbuf)/8 times */
		if (WCHAR_UTF16 && cp >= 0x10000) {
			*ws++ = SURR_HIGH(cp);
			if (wn >= 2) {
				*ws++ = SURR_LOW(cp);
				wn -= 2;
				cnt += 2;
			} else {
				/* one slot (a resumed pair): park the low half; the next call
				 * drains it at the top. */
				((unsigned *)st)[1] = SURR_LOW(cp);
				wn--;
				cnt++;
				break;
			}
		} else {
			*ws++ = cp;
			wn--;
			cnt++;
		}
	}
	if (wcs) *src = s;
	return cnt;
}

c2go_extern size_t wcsnrtombs(char *restrict dst, const wchar_t **restrict wcs, size_t wn, size_t n, mbstate_t *restrict st) {
	const wchar_t *ws = *wcs;
	size_t cnt = 0;
	if (!dst) n=0;
	while (ws && wn) {
		char tmp[MB_LEN_MAX];
		size_t l;
		/* UTF-16: a supplementary scalar is a high+low surrogate PAIR. Forming it
		 * costs a SECOND input wide char, so if the input bound (wn) does not
		 * cover it the high half is not proven unpaired — stop without consuming,
		 * restartable. Within the bound, a high not followed by a low is a genuine
		 * EILSEQ; a lone low falls through to wcrtomb, which also rejects it. */
		if (WCHAR_UTF16 && IS_HIGH_SURR(*ws)) {
			if (wn < 2) break;
			if (!IS_LOW_SURR(ws[1])) { errno = EILSEQ; cnt = -1; break; }
			l = 4;
			__surrogate_to_utf8(n<MB_LEN_MAX ? tmp : dst, ws[0], ws[1]);
			if (dst) {
				if (n<MB_LEN_MAX) {
					if (l>n) break;
					memcpy(dst, tmp, l);
				}
				dst += l;
				n -= l;
			}
			ws += 2;
			wn -= 2;
			cnt += l;
			continue;
		}
		l = wcrtomb(n<MB_LEN_MAX ? tmp : dst, *ws, 0);
		if (l==-1) {
			cnt = -1;
			break;
		}
		if (dst) {
			if (n<MB_LEN_MAX) {
				if (l>n) break;
				memcpy(dst, tmp, l);
			}
			dst += l;
			n -= l;
		}
		if (!*ws) {
			ws = 0;
			break;
		}
		ws++;
		wn--;
		cnt += l;
	}
	if (dst) *wcs = ws;
	return cnt;
}
