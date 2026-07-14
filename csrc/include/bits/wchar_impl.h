/* bits/wchar_impl.h — internal UTF-16 surrogate helpers shared by the wide
 * conversion layer (multibyte.c) and the wide FILE I/O (stdio.c). Only
 * c2go-libc's own .c files include this. On Windows wchar_t is a uint16 UTF-16
 * code unit, so a supplementary scalar (>= U+10000) is a surrogate PAIR of two
 * wchar_t; on the unix targets wchar_t is int32 and WCHAR_UTF16 is a constant 0
 * that folds every surrogate branch away. */
#ifndef _BITS_WCHAR_IMPL_H
#define _BITS_WCHAR_IMPL_H

#include <wchar.h>   /* wchar_t */

/* Compile-time constant: 1 iff wchar_t is a 16-bit UTF-16 code unit. */
#define WCHAR_UTF16   (sizeof(wchar_t) < 4)

/* A supplementary scalar cp (0x10000..0x10FFFF) splits into
 *   high = 0xd7c0 + (cp >> 10)      [== 0xd800 + ((cp-0x10000) >> 10)]
 *   low  = 0xdc00 + (cp & 0x3ff)   */
#define SURR_HIGH(cp) (0xd7c0u + ((unsigned)(cp) >> 10))
#define SURR_LOW(cp)  (0xdc00u + ((unsigned)(cp) & 0x3ffu))
#define IS_HIGH_SURR(w) ((unsigned)(w) - 0xd800u < 0x400u)
#define IS_LOW_SURR(w)  ((unsigned)(w) - 0xdc00u < 0x400u)

/* A supplementary scalar is EXACTLY a 4-byte UTF-8 sequence (lead 0xf0..0xf4);
 * the string converters peek the lead to reserve output room for a pair. */
#define IS_4BYTE_LEAD(b) ((unsigned char)(b) - 0xf0u < 5u)

/* Combine a validated high+low surrogate pair into 4 UTF-8 bytes (defined in
 * source/multibyte.c). Exported so the printf/scanf wide conversions in stdio.c
 * can encode a supplementary pair the 16-bit wctomb/wcrtomb cannot. */
size_t __surrogate_to_utf8(char *, unsigned, unsigned)
    c2go_linkname("github.com/c2gohq/c2go_libc.__surrogate_to_utf8", C2GO_GOABI0);

#endif /* _BITS_WCHAR_IMPL_H */
