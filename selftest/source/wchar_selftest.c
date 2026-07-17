/* wchar_selftest.c — BMP-level wide-character exercise. Driven on unix by
 * selftest/wchar_test.go and on Windows (where wchar_t is uint16/UTF-16) under
 * wine by .build_status/wchar_windows_e2e_gate.sh.
 *
 * The BMP steps (<= U+FFFF, where ONE wchar_t holds ONE code point on both the
 * unix int32/UTF-32 and Windows uint16/UTF-16 targets) use wchar-width-independent
 * expectations. The supplementary-plane steps (> U+FFFF) branch on sizeof(wchar_t):
 * one scalar on unix, a surrogate PAIR on Windows — so each target checks its own
 * ABI. Together they cover the wide-character subsystem (multibyte.c conversions,
 * wstring.c, wctype.c, the wide FILE I/O and the printf/scanf wide conversions in
 * stdio.c). wint_t is a 32-bit `unsigned` on every target, so the classification
 * checks are width-independent too.
 *
 * Buffer fill/compare are inlined and mbstate_t is zero-initialised (no memset/
 * memcmp) so the test stays self-contained: those are compiler-intercepted and
 * their Windows dynamic-import glue is not linked into a standalone selftest —
 * same reason as pathsig_selftest.c. A completed conversion leaves *st in the
 * initial state, so a single zero-init is reused across the mbrtowc calls.
 *
 * UTF-8 byte inputs and wide values are written as explicit hex so the test does
 * not depend on the source file's execution charset. Returns 0 on success, or
 * the failing step number. */
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>   /* mbtowc / wctomb / mbstowcs / wcstombs */
#include <stdio.h>    /* tmpfile / fputwc / fgetwc for the wide stdio steps */
#include <time.h>     /* struct tm for the wcsftime step (#652) */
#include <monetary.h> /* strfmon (#652) */
#include <libintl.h>  /* gettext family (#652) */
#include <c2go.h>

c2go_extern int WcharSelftest(void)
{
	mbstate_t st = {0};
	wchar_t wc;
	char mb[8];

	/* 1: mbrtowc decodes "é" (U+00E9, UTF-8 C3 A9) to one wchar, 2 bytes. */
	if (mbrtowc(&wc, "\xC3\xA9", 8, &st) != 2 || wc != 0x00E9) return 1;

	/* 2: mbrtowc decodes "中" (U+4E2D, UTF-8 E4 B8 AD), 3 bytes. */
	if (mbrtowc(&wc, "\xE4\xB8\xAD", 8, &st) != 3 || wc != 0x4E2D) return 2;

	/* 3: wcrtomb encodes U+00E9 back to C3 A9. */
	if (wcrtomb(mb, 0x00E9, &st) != 2 ||
	    (unsigned char)mb[0] != 0xC3 || (unsigned char)mb[1] != 0xA9) return 3;

	/* 4: wcrtomb encodes U+4E2D to E4 B8 AD. */
	if (wcrtomb(mb, 0x4E2D, &st) != 3 ||
	    (unsigned char)mb[0] != 0xE4 || (unsigned char)mb[1] != 0xB8 ||
	    (unsigned char)mb[2] != 0xAD) return 4;

	/* 5: mbstowcs "aé中" -> 3 wide chars [0x61,0xE9,0x4E2D]; wcslen == 3. */
	wchar_t ws[8];
	if (mbstowcs(ws, "a\xC3\xA9\xE4\xB8\xAD", 8) != 3) return 5;
	if (ws[0] != 0x61 || ws[1] != 0x00E9 || ws[2] != 0x4E2D || ws[3] != 0) return 5;
	if (wcslen(ws) != 3) return 5;

	/* 6: wcstombs back to the original 6 UTF-8 bytes (1 + 2 + 3). */
	char out[16];
	if (wcstombs(out, ws, sizeof out) != 6) return 6;
	static const unsigned char want6[6] = { 0x61, 0xC3, 0xA9, 0xE4, 0xB8, 0xAD };
	for (int i = 0; i < 6; i++)
		if ((unsigned char)out[i] != want6[i]) return 6;

	/* 7: wcscpy / wcscmp / wcschr over the BMP wide string. */
	wchar_t cp[8];
	wcscpy(cp, ws);
	if (wcscmp(cp, ws) != 0) return 7;
	if (wcschr(ws, 0x4E2D) != &ws[2]) return 7;

	/* 8: wctype classification / case mapping of BMP code points. */
	if (!iswalpha(0x4E2D)) return 8;            /* 中 is a letter          */
	if (!iswalpha(0x00E9)) return 8;            /* é is a letter           */
	if (iswdigit(0x00E9)) return 8;
	if (towlower(0x00C9) != 0x00E9) return 8;   /* É -> é                  */
	if (towupper(0x00E9) != 0x00C9) return 8;   /* é -> É                  */

	/* 9: mbtowc / wctomb (non-restartable) BMP roundtrip. */
	if (mbtowc(&wc, "\xC3\xA9", 8) != 2 || wc != 0x00E9) return 9;
	if (wctomb(mb, 0x00E9) != 2 || (unsigned char)mb[0] != 0xC3) return 9;

	/* ── Supplementary plane (> U+FFFF): U+1F600 (UTF-8 F0 9F 98 80). On unix
	 * (UTF-32) one wchar_t holds it; on Windows (UTF-16) it is the surrogate
	 * PAIR high=0xD83D low=0xDE00. Expectations branch on sizeof(wchar_t) — the
	 * compiler folds the dead arm, so each target checks exactly its own ABI. */

	/* 10: mbstowcs of the supplementary scalar -> 1 wchar (unix) / 2 (Windows). */
	wchar_t sw[8];
	size_t n10 = mbstowcs(sw, "\xF0\x9F\x98\x80", 8);
	if (sizeof(wchar_t) >= 4) {
		if (n10 != 1 || sw[0] != 0x1F600 || sw[1] != 0) return 10;
	} else {
		if (n10 != 2 || sw[0] != 0xD83D || sw[1] != 0xDE00 || sw[2] != 0) return 10;
	}

	/* 11: wcstombs back — input shape differs per target, output is the 4 bytes. */
	char so[8];
	if (wcstombs(so, sw, sizeof so) != 4) return 11;
	static const unsigned char want11[4] = { 0xF0, 0x9F, 0x98, 0x80 };
	for (int i = 0; i < 4; i++)
		if ((unsigned char)so[i] != want11[i]) return 11;

	/* 12: single-char mbrtowc — one wchar on unix; on Windows a single wchar_t
	 * cannot hold a supplementary scalar, so it reports EILSEQ (use the string
	 * converters or mbrtoc16 for supplementary text). */
	mbstate_t st2 = {0};
	wchar_t w12;
	size_t r12 = mbrtowc(&w12, "\xF0\x9F\x98\x80", 8, &st2);
	if (sizeof(wchar_t) >= 4) {
		if (r12 != 4 || w12 != 0x1F600) return 12;
	} else {
		if (r12 != (size_t)-1) return 12;
	}

	/* 13: single-char wide->byte — unix encodes the scalar (4 bytes); on Windows
	 * a lone surrogate half has no UTF-8 encoding -> EILSEQ. */
	char sb[8];
	mbstate_t st3 = {0};
	if (sizeof(wchar_t) >= 4) {
		if (wcrtomb(sb, (wchar_t)0x1F600, &st3) != 4 ||
		    (unsigned char)sb[0] != 0xF0 || (unsigned char)sb[3] != 0x80) return 13;
	} else {
		if (wcrtomb(sb, (wchar_t)0xD83D, &st3) != (size_t)-1) return 13;
	}

	/* 14: bounded n-variants. On Windows wn=1 has no room for the 2-unit pair, so
	 * the conversion produces nothing and is restartable (exercises the "no room
	 * for the pair yet" tail path); a follow-up call with room lands the pair. */
	const char *sp = "\xF0\x9F\x98\x80";
	mbstate_t st4 = {0};
	wchar_t wb[4];
	if (sizeof(wchar_t) >= 4) {
		if (mbsnrtowcs(wb, &sp, 4, 4, &st4) != 1 || wb[0] != 0x1F600) return 14;
	} else {
		if (mbsnrtowcs(wb, &sp, 4, 1, &st4) != 0) return 14;           /* no room */
		if (mbsnrtowcs(wb, &sp, 4, 4, &st4) != 2 ||
		    wb[0] != 0xD83D || wb[1] != 0xDE00) return 14;             /* now fits */
	}
	mbstate_t st5 = {0};
	const wchar_t *wp = wb;
	char cb[8];
	size_t wn5 = (sizeof(wchar_t) >= 4) ? 1 : 2;
	if (wcsnrtombs(cb, &wp, wn5, sizeof cb, &st5) != 4 ||
	    (unsigned char)cb[0] != 0xF0 || (unsigned char)cb[3] != 0x80) return 14;

	/* 14b: the same via the internal (st==NULL) fallback state — its 2nd word
	 * must exist (a single-word fallback would touch memory out of bounds on the
	 * UTF-16 drain check). */
	const char *sp2 = "\xF0\x9F\x98\x80";
	wchar_t wb2[4];
	size_t r14b = mbsnrtowcs(wb2, &sp2, 4, 4, NULL);
	if (sizeof(wchar_t) >= 4) {
		if (r14b != 1 || wb2[0] != 0x1F600) return 14;
	} else {
		if (r14b != 2 || wb2[0] != 0xD83D || wb2[1] != 0xDE00) return 14;
	}

	/* 15: wide stdio round-trip through a temp stream. fputwc 'A', é, 中, then the
	 * supplementary U+1F600 (one wchar on unix; a high+low surrogate PAIR of fputwc
	 * calls on Windows); the stream must hold the 10 UTF-8 bytes. Reading them back
	 * with fgetwc yields the same wide units — a surrogate pair on Windows, where
	 * fgetwc splits the scalar and buffers the low half for the next call.
	 * step 15 = tmpfile unavailable; 16 = wrong bytes written; 17/18 = wrong wide
	 * units read back. */
	FILE *tf = tmpfile();
	if (!tf) return 15;
	fputwc(0x41, tf);
	fputwc(0x00E9, tf);
	fputwc(0x4E2D, tf);
	if (sizeof(wchar_t) >= 4) {
		fputwc(0x1F600, tf);
	} else {
		fputwc(0xD83D, tf);   /* high surrogate: buffered         */
		fputwc(0xDE00, tf);   /* low surrogate: emits the 4 bytes */
	}
	fflush(tf);
	rewind(tf);
	unsigned char raw[16];
	size_t nr = fread(raw, 1, sizeof raw, tf);
	static const unsigned char want15[10] =
		{ 0x41, 0xC3, 0xA9, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80 };
	if (nr != 10) { fclose(tf); return 16; }
	for (int i = 0; i < 10; i++)
		if (raw[i] != want15[i]) { fclose(tf); return 16; }
	rewind(tf);
	if (fgetwc(tf) != 0x41)   { fclose(tf); return 17; }
	if (fgetwc(tf) != 0x00E9) { fclose(tf); return 17; }
	if (fgetwc(tf) != 0x4E2D) { fclose(tf); return 17; }
	if (sizeof(wchar_t) >= 4) {
		if (fgetwc(tf) != 0x1F600) { fclose(tf); return 18; }
	} else {
		if (fgetwc(tf) != 0xD83D) { fclose(tf); return 18; }   /* high first */
		if (fgetwc(tf) != 0xDE00) { fclose(tf); return 18; }   /* then low   */
	}
	if (fgetwc(tf) != WEOF) { fclose(tf); return 18; }
	fclose(tf);

	/* 19-25: ungetwc round-trip. Push a BMP wide char back and re-read it (both
	 * targets); on Windows also push a lone surrogate half, which has no byte
	 * encoding and is parked in the same pending slot fgetwc drains. */
	FILE *tu = tmpfile();
	if (!tu) return 19;
	fwrite("Z", 1, 1, tu);
	rewind(tu);
	if (fgetwc(tu) != 0x5A) { fclose(tu); return 20; }      /* 'Z' */
	if (ungetwc(0x00E9, tu) != 0x00E9) { fclose(tu); return 21; }  /* push é */
	if (fgetwc(tu) != 0x00E9) { fclose(tu); return 22; }    /* re-read é */
	if (sizeof(wchar_t) < 4) {
		if (ungetwc(0xD83D, tu) != 0xD83D) { fclose(tu); return 23; }  /* push high */
		if (fgetwc(tu) != 0xD83D) { fclose(tu); return 24; }           /* re-read it */
	}
	fclose(tu);

	/* ── formatted I/O over the BMP (printf/scanf %ls,%lc,%s + wide %s,%ls). Exercises
	 * the wide conversions inside printf_core / vfscanf / wprintf_core / vfwscanf and
	 * the swprintf sw_write / swscanf wstring_read wchar<->byte sinks. Every value is
	 * <= U+FFFF (one wchar_t per code point on both targets). 26 = narrow printf, 27 =
	 * narrow scanf, 28 = wide printf, 29 = wide scanf. ASCII-only wide format literals
	 * (L"%s") are charset-independent; the non-ASCII data stays explicit hex. */

	/* 26: narrow snprintf %ls (wide -> UTF-8, printf_core wctomb) and %lc. */
	wchar_t pw26[] = { 0x61, 0x00E9, 0x4E2D, 0 };   /* a é 中 */
	char pb26[16];
	if (snprintf(pb26, sizeof pb26, "%ls", pw26) != 6) return 26;
	static const unsigned char want26[6] = { 0x61, 0xC3, 0xA9, 0xE4, 0xB8, 0xAD };
	for (int i = 0; i < 6; i++) if ((unsigned char)pb26[i] != want26[i]) return 26;
	if (snprintf(pb26, sizeof pb26, "%lc", (wint_t)0x00E9) != 2 ||
	    (unsigned char)pb26[0] != 0xC3 || (unsigned char)pb26[1] != 0xA9) return 26;

	/* 27: narrow sscanf %ls (UTF-8 -> wide, vfscanf mbrtowc). */
	wchar_t sd27[8];
	if (sscanf("a\xC3\xA9\xE4\xB8\xAD", "%ls", sd27) != 1) return 27;
	if (sd27[0] != 0x61 || sd27[1] != 0x00E9 || sd27[2] != 0x4E2D || sd27[3] != 0) return 27;

	/* 28: wide swprintf %s (UTF-8 -> wide, wprintf_core mbtowc) and %ls (wide -> wide),
	 * both through the sw_write wchar sink (mbtowc re-decode of the UTF-8 scratch). */
	wchar_t pw28[16];
	if (swprintf(pw28, 16, L"%s", "a\xC3\xA9\xE4\xB8\xAD") != 3) return 28;
	if (pw28[0] != 0x61 || pw28[1] != 0x00E9 || pw28[2] != 0x4E2D || pw28[3] != 0) return 28;
	wchar_t ps28[] = { 0x61, 0x00E9, 0x4E2D, 0 };
	if (swprintf(pw28, 16, L"%ls", ps28) != 3) return 28;
	if (pw28[0] != 0x61 || pw28[1] != 0x00E9 || pw28[2] != 0x4E2D || pw28[3] != 0) return 28;

	/* 29: wide swscanf %ls (wide -> wide) and %s (wide -> UTF-8, vfwscanf getwc+wctomb);
	 * the wide input is re-encoded to UTF-8 by wstring_read/wcsrtombs first. */
	wchar_t sw29[] = { 0x61, 0x00E9, 0x4E2D, 0 };
	wchar_t sd29[8];
	if (swscanf(sw29, L"%ls", sd29) != 1) return 29;
	if (sd29[0] != 0x61 || sd29[1] != 0x00E9 || sd29[2] != 0x4E2D || sd29[3] != 0) return 29;
	char sb29[16];
	if (swscanf(sw29, L"%s", sb29) != 1) return 29;
	static const unsigned char want29[6] = { 0x61, 0xC3, 0xA9, 0xE4, 0xB8, 0xAD };
	for (int i = 0; i < 6; i++) if ((unsigned char)sb29[i] != want29[i]) return 29;

	/* ── formatted I/O, supplementary plane (U+1F600 😀; UTF-8 F0 9F 98 80). One
	 * wchar_t holds the scalar on unix; on Windows it is the surrogate PAIR
	 * high=0xD83D low=0xDE00, so wide inputs/outputs branch on sizeof(wchar_t). This
	 * drives the surrogate combine/split in printf_core (Site A) / vfscanf (B) /
	 * wprintf_core (C) / vfwscanf (D) and the swprintf sw_write sink (E). 30 = narrow
	 * printf, 31 = narrow scanf, 32/34/35 = wide printf (last two: precision +
	 * truncation edges), 33 = wide scanf. */

	/* 30: narrow snprintf %ls of a supplementary -> the 5 UTF-8 bytes (Site A pairs
	 * across the two passes). %lc of a supplementary wint_t works on BOTH targets:
	 * the %C case splits the 32-bit scalar into a surrogate pair on Windows. */
	wchar_t pw30[6];
	pw30[0] = 0x61;
	if (sizeof(wchar_t) >= 4) { pw30[1] = 0x1F600; pw30[2] = 0; }
	else { pw30[1] = 0xD83D; pw30[2] = 0xDE00; pw30[3] = 0; }
	char pb30[16];
	if (snprintf(pb30, sizeof pb30, "%ls", pw30) != 5) return 30;
	static const unsigned char want30[5] = { 0x61, 0xF0, 0x9F, 0x98, 0x80 };
	for (int i = 0; i < 5; i++) if ((unsigned char)pb30[i] != want30[i]) return 30;
	char pb30b[8];
	if (snprintf(pb30b, sizeof pb30b, "%lc", (wint_t)0x1F600) != 4 ||
	    (unsigned char)pb30b[0] != 0xF0 || (unsigned char)pb30b[1] != 0x9F ||
	    (unsigned char)pb30b[2] != 0x98 || (unsigned char)pb30b[3] != 0x80) return 30;

	/* 31: narrow sscanf %ls of the 4 UTF-8 bytes -> one scalar (unix) / a pair (Win)
	 * (Site B: __mbrtoc32 reaches the scalar, emits the surrogate units). */
	wchar_t sd31[8];
	if (sscanf("a\xF0\x9F\x98\x80", "%ls", sd31) != 1) return 31;
	if (sizeof(wchar_t) >= 4) {
		if (sd31[0] != 0x61 || sd31[1] != 0x1F600 || sd31[2] != 0) return 31;
	} else {
		if (sd31[0] != 0x61 || sd31[1] != 0xD83D || sd31[2] != 0xDE00 || sd31[3] != 0) return 31;
	}

	/* 32: wide swprintf %s (Site C source-decode + Site E sink) and %ls (Site E) of a
	 * supplementary. The return counts UTF-16 UNITS: 2 on unix (a + scalar), 3 on Win
	 * (a + high + low). */
	wchar_t pw32[16];
	int r32 = swprintf(pw32, 16, L"%s", "a\xF0\x9F\x98\x80");
	if (sizeof(wchar_t) >= 4) {
		if (r32 != 2 || pw32[0] != 0x61 || pw32[1] != 0x1F600 || pw32[2] != 0) return 32;
	} else {
		if (r32 != 3 || pw32[0] != 0x61 || pw32[1] != 0xD83D || pw32[2] != 0xDE00 || pw32[3] != 0) return 32;
	}
	wchar_t ps32[6];
	int n32 = 0;
	ps32[n32++] = 0x61;
	if (sizeof(wchar_t) >= 4) ps32[n32++] = 0x1F600;
	else { ps32[n32++] = 0xD83D; ps32[n32++] = 0xDE00; }
	ps32[n32] = 0;
	int r32b = swprintf(pw32, 16, L"%ls", ps32);
	if (sizeof(wchar_t) >= 4) {
		if (r32b != 2 || pw32[0] != 0x61 || pw32[1] != 0x1F600 || pw32[2] != 0) return 32;
	} else {
		if (r32b != 3 || pw32[0] != 0x61 || pw32[1] != 0xD83D || pw32[2] != 0xDE00 || pw32[3] != 0) return 32;
	}

	/* 33: wide swscanf %ls (surrogate units) and %s (bytes) of a supplementary. The
	 * wide input carries the pair; %s round-trips it to the 5 UTF-8 bytes (Site D
	 * combines the getwc halves). */
	wchar_t sw33[6];
	int n33 = 0;
	sw33[n33++] = 0x61;
	if (sizeof(wchar_t) >= 4) sw33[n33++] = 0x1F600;
	else { sw33[n33++] = 0xD83D; sw33[n33++] = 0xDE00; }
	sw33[n33] = 0;
	wchar_t sd33[8];
	if (swscanf(sw33, L"%ls", sd33) != 1) return 33;
	if (sizeof(wchar_t) >= 4) {
		if (sd33[0] != 0x61 || sd33[1] != 0x1F600 || sd33[2] != 0) return 33;
	} else {
		if (sd33[0] != 0x61 || sd33[1] != 0xD83D || sd33[2] != 0xDE00 || sd33[3] != 0) return 33;
	}
	char sb33[16];
	if (swscanf(sw33, L"%s", sb33) != 1) return 33;
	static const unsigned char want33[5] = { 0x61, 0xF0, 0x9F, 0x98, 0x80 };
	for (int i = 0; i < 5; i++) if ((unsigned char)sb33[i] != want33[i]) return 33;

	/* 34: swprintf truncation — on Windows the 2-unit pair does not fit n=2 (one slot
	 * + NUL), so Site E stores nothing (no split) and vswprintf returns -1; on unix
	 * the single scalar fits and it returns 1. */
	wchar_t tb34[2];
	int r34 = swprintf(tb34, 2, L"%s", "\xF0\x9F\x98\x80");
	if (sizeof(wchar_t) >= 4) { if (r34 != 1 || tb34[0] != 0x1F600 || tb34[1] != 0) return 34; }
	else { if (r34 != -1) return 34; }

	/* 35: precision boundary — %.1s caps output at 1 UTF-16 unit. On Windows the
	 * supplementary needs 2 units so nothing is emitted (never a lone surrogate);
	 * on unix the single scalar fits. */
	wchar_t pb35[8];
	int r35 = swprintf(pb35, 8, L"%.1s", "\xF0\x9F\x98\x80");
	if (sizeof(wchar_t) >= 4) { if (r35 != 1 || pb35[0] != 0x1F600) return 35; }
	else { if (r35 != 0 || pb35[0] != 0) return 35; }

	/* 36: precision robustness (adversarial-review findings). (a) wide %.3s of "abc"
	 * followed by an ILLEGAL byte must truncate cleanly to "abc" (3 units), NOT
	 * abort with -1 — the measure loop applies the precision cap before decoding the
	 * boundary byte (musl's l<p short-circuit; both targets). (b) wide %.1S of a
	 * surrogate pair must drop the unpaired half, never emit a lone high surrogate
	 * that would poison wpend and swallow later output — the trailing '|' survives. */
	wchar_t eb36[16];
	if (swprintf(eb36, 16, L"%.3s", "abc\xff") != 3 ||
	    eb36[0] != 0x61 || eb36[1] != 0x62 || eb36[2] != 0x63 || eb36[3] != 0) return 36;
	wchar_t sp36[4];
	int m36 = 0;
	if (sizeof(wchar_t) >= 4) sp36[m36++] = 0x1F600;
	else { sp36[m36++] = 0xD83D; sp36[m36++] = 0xDE00; }
	sp36[m36] = 0;
	wchar_t ob36[16];
	int r36 = swprintf(ob36, 16, L"%.1S|", sp36);
	if (sizeof(wchar_t) >= 4) {
		if (r36 != 2 || ob36[0] != 0x1F600 || ob36[1] != 0x7C || ob36[2] != 0) return 36;
	} else {
		if (r36 != 1 || ob36[0] != 0x7C || ob36[1] != 0) return 36;
	}

	/* 37: wide scanf %[ with a supplementary scanset member (task #632). On Windows
	 * the scanset literal is a surrogate PAIR; matching only the high half wrongly
	 * accepted a DIFFERENT supplementary sharing that high surrogate. Format built
	 * explicitly (charset-independent). */
	wchar_t fmt37[8];
	int q37 = 0;
	fmt37[q37++] = '%'; fmt37[q37++] = '[';
	if (sizeof(wchar_t) >= 4) fmt37[q37++] = 0x1F600;
	else { fmt37[q37++] = 0xD83D; fmt37[q37++] = 0xDE00; }
	fmt37[q37++] = ']'; fmt37[q37] = 0;

	/* 37a: a NON-member supplementary (U+1F400 shares high 0xD83D on Windows) must
	 * NOT match — before the fix its low half was never re-checked and it was
	 * wrongly accepted (return 1). */
	wchar_t in37a[4];
	int p37a = 0;
	if (sizeof(wchar_t) >= 4) in37a[p37a++] = 0x1F400;
	else { in37a[p37a++] = 0xD83D; in37a[p37a++] = 0xDC00; }
	in37a[p37a] = 0;
	char buf37a[16];
	if (swscanf(in37a, fmt37, buf37a) == 1) return 37;

	/* 37b: the MEMBER supplementary (U+1F600) matches and round-trips to 4 UTF-8 bytes. */
	wchar_t in37b[4];
	int p37b = 0;
	if (sizeof(wchar_t) >= 4) in37b[p37b++] = 0x1F600;
	else { in37b[p37b++] = 0xD83D; in37b[p37b++] = 0xDE00; }
	in37b[p37b] = 0;
	char buf37b[16];
	if (swscanf(in37b, fmt37, buf37b) != 1) return 37;
	static const unsigned char want37b[4] = { 0xF0, 0x9F, 0x98, 0x80 };
	for (int i = 0; i < 4; i++) if ((unsigned char)buf37b[i] != want37b[i]) return 37;

	/* 37c: %[A] stops at a non-member supplementary and LEAVES the whole pair intact
	 * for the next %ls (exercises __ungetwc_pair restoring the pushed-back pair). */
	wchar_t in37c[8];
	int p37c = 0;
	in37c[p37c++] = 0x41;   /* 'A' */
	if (sizeof(wchar_t) >= 4) in37c[p37c++] = 0x1F400;
	else { in37c[p37c++] = 0xD83D; in37c[p37c++] = 0xDC00; }
	in37c[p37c] = 0;
	char ab37c[8];
	wchar_t wb37c[8];
	if (swscanf(in37c, L"%[A]%ls", ab37c, wb37c) != 2) return 37;
	if (ab37c[0] != 0x41 || ab37c[1] != 0) return 37;
	if (sizeof(wchar_t) >= 4) {
		if (wb37c[0] != 0x1F400 || wb37c[1] != 0) return 37;
	} else {
		if (wb37c[0] != 0xD83D || wb37c[1] != 0xDC00 || wb37c[2] != 0) return 37;
	}

	/* 38: wcsftime (#652) — wide strftime: a conversion plus a non-ASCII wide
	 * literal (U+65E5 日, BMP so ONE unit on both targets). The fields are set
	 * by hand: 1970-01-01, a Thursday. */
	{
		struct tm tm38 = {0};
		tm38.tm_year = 70; tm38.tm_mday = 1; tm38.tm_wday = 4;
		wchar_t f38[8], out38[32];
		int p38 = 0;
		f38[p38++] = 0x65E5;  /* 日 */
		f38[p38++] = '=';
		f38[p38++] = '%'; f38[p38++] = 'Y';
		f38[p38] = 0;
		if (wcsftime(out38, 32, f38, &tm38) != 6) return 38;
		if (out38[0] != 0x65E5 || out38[1] != '=' || out38[2] != '1' ||
		    out38[3] != '9' || out38[4] != '7' || out38[5] != '0' || out38[6] != 0)
			return 38;
		if (wcsftime(out38, 4, f38, &tm38) != 0) return 38; /* overflow -> 0 */
	}

	/* 39: strfmon (#652) — C-locale monetary formatting is plain numeric. */
	{
		char b39[32];
		ssize_t n39 = strfmon(b39, sizeof b39, "%.2n", 12.25);
		if (n39 != 5) return 39;
		if (b39[0] != '1' || b39[1] != '2' || b39[2] != '.' ||
		    b39[3] != '2' || b39[4] != '5' || b39[5] != 0)
			return 39;
	}

	/* 40: gettext (#652) — no catalogs, so the msgid pointer comes back as-is
	 * and ngettext selects by n. */
	{
		static const char one40[] = "one", many40[] = "many";
		if (gettext(one40) != one40) return 40;
		if (ngettext(one40, many40, 1) != one40) return 40;
		if (ngettext(one40, many40, 2) != many40) return 40;
	}

	/* 41: supplementary followed by ASCII (#655 H1) — the wide-scanf getwc
	 * fast path must yield to the PARKED low surrogate: after a supplementary
	 * decode on UTF-16 targets, an ASCII byte at rpos must not jump the queue.
	 * Regression shape: %ls delivered the pair out of order, %s input_failed. */
	{
		wchar_t in41[8];
		int p41 = 0;
		if (sizeof(wchar_t) >= 4) in41[p41++] = 0x1F600;
		else { in41[p41++] = 0xD83D; in41[p41++] = 0xDE00; }
		in41[p41++] = 0x41; /* 'A' */
		in41[p41] = 0;

		wchar_t wb41[8];
		if (swscanf(in41, L"%ls", wb41) != 1) return 41;
		if (sizeof(wchar_t) >= 4) {
			if (wb41[0] != 0x1F600 || wb41[1] != 0x41 || wb41[2] != 0) return 41;
		} else {
			if (wb41[0] != 0xD83D || wb41[1] != 0xDE00 || wb41[2] != 0x41 || wb41[3] != 0) return 41;
		}

		char sb41[16];
		if (swscanf(in41, L"%s", sb41) != 1) return 41;
		static const unsigned char want41[5] = { 0xF0, 0x9F, 0x98, 0x80, 0x41 };
		for (int i = 0; i < 5; i++) if ((unsigned char)sb41[i] != want41[i]) return 41;
		if (sb41[5] != 0) return 41;
	}

	/* 42: resumed-supplementary protocols (#689/#690, observable only where
	 * wchar_t is 16-bit — this step is the wine gate's reason to exist).
	 * (a) #689: a counting pass (dst==NULL) that RESUMES a partially-decoded
	 *     supplementary must count it at wchar width (2 units on UTF-16; the
	 *     resumed path used to count 1, under-sizing callers' buffers).
	 * (b) #690: a bounded pass (wn==1) resuming into a supplementary must
	 *     STALL — consume nothing, park nothing — and src+state must then be
	 *     resumable by the OTHER string engine (mbsrtowcs) into the complete
	 *     pair (the old parked-low state was private to mbsnrtowcs and the
	 *     pair's low half was lost cross-engine). */
	{
		/* (a) prime the state with the first 2 bytes of U+1F600, then count
		 * the remainder plus a trailing ASCII. */
		mbstate_t st42 = {0};
		wchar_t wc42;
		if (mbrtowc(&wc42, "\xF0\x9F", 2, &st42) != (size_t)-2) return 42;
		const char *sp42 = "\x98\x80" "A";
		size_t n42 = mbsrtowcs(NULL, &sp42, 0, &st42);
		if (sizeof(wchar_t) >= 4) {
			if (n42 != 2) return 42;                 /* U+1F600, 'A' */
		} else {
			if (n42 != 3) return 42;                 /* high, low, 'A' */
		}

		/* (b) same priming; bounded resume with wn==1, then the other engine. */
		mbstate_t st42b = {0};
		if (mbrtowc(&wc42, "\xF0\x9F", 2, &st42b) != (size_t)-2) return 42;
		const char *sp42b = "\x98\x80" "A";
		wchar_t wb42[4];
		if (sizeof(wchar_t) >= 4) {
			if (mbsnrtowcs(wb42, &sp42b, 3, 1, &st42b) != 1 || wb42[0] != 0x1F600)
				return 42;
		} else {
			if (mbsnrtowcs(wb42, &sp42b, 3, 1, &st42b) != 0) return 42; /* stall */
			size_t r42 = mbsrtowcs(wb42, &sp42b, 4, &st42b);
			if (r42 != 3 || wb42[0] != 0xD83D || wb42[1] != 0xDE00 || wb42[2] != 0x41)
				return 42;                           /* cross-engine full pair */
		}
	}

	return 0;
}
