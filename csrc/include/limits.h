/* limits.h — implementation limits. Integer maxima come from clang's
 * target-aware builtins, so e.g. LONG_MAX is correct on both LP64 and
 * LLP64 without a per-target header. The path/pipe limits are NATIVE
 * per-OS (buffer-size hints; the Go bridge / OS enforces the real limit).
 * See ../PORTABILITY.md. */
#ifndef _LIMITS_H
#define _LIMITS_H

#define CHAR_BIT   8
/* Max bytes per multibyte character. The C locale is UTF-8 on every c2go
 * target (a supplementary scalar is a 4-byte sequence), so 4 — musl's value.
 * MUST live here: without it, <limits.h> resolves to clang's resource header
 * on the windows-goabi triple, whose freestanding fallback pins MB_LEN_MAX
 * to 1 — every `char buf[MB_LEN_MAX]` conversion buffer then under-allocates
 * and wcrtomb/wctomb overflow into the adjacent stack slot (silent pointer
 * corruption, found via the ungetwc wine gate). */
#define MB_LEN_MAX 4
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN   0
#define CHAR_MAX   UCHAR_MAX
#else
#define CHAR_MIN   SCHAR_MIN
#define CHAR_MAX   SCHAR_MAX
#endif

#define SHRT_MIN   (-1-0x7fff)
#define SHRT_MAX   0x7fff
#define USHRT_MAX  0xffff

#define INT_MIN    (-1-__INT_MAX__)
#define INT_MAX    __INT_MAX__
#define UINT_MAX   (__INT_MAX__ * 2u + 1u)

#define LONG_MIN   (-LONG_MAX-1L)
#define LONG_MAX   __LONG_MAX__
#define ULONG_MAX  (LONG_MAX * 2uL + 1uL)

#define LLONG_MIN  (-LLONG_MAX-1LL)
#define LLONG_MAX  __LONG_LONG_MAX__
#define ULLONG_MAX (LLONG_MAX * 2uLL + 1uLL)

/* ssize_t is pointer-sized (== intptr_t) on every c2go target, so its maximum
 * is the intptr max — NOT LONG_MAX, which is only 4 bytes on Windows LLP64. */
#define SSIZE_MAX  __INTPTR_MAX__

/* path / pipe limits — native per-OS (NAME_MAX is 255 on all three). */
#define NAME_MAX   255
#if defined(__linux__)
#define PATH_MAX   4096
#define PIPE_BUF   4096
#elif defined(__APPLE__)
#define PATH_MAX   1024
#define PIPE_BUF   512
#elif defined(_WIN32)
#define PATH_MAX   260    /* MinGW MAX_PATH; Go bridge handles Windows long paths */
#define PIPE_BUF   512
#else
#error "unsupported c2go OS"
#endif

/* POSIX regex limits (musl values; consumed by regcomp #667). */
#define CHARCLASS_NAME_MAX 14
#define RE_DUP_MAX 255

#endif /* _LIMITS_H */
