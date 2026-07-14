/* inttypes.h — format macros + greatest-width integer conversion. The PRI and
 * SCN strings come from clang's target builtins (e.g. PRId64 is "ld" on LP64,
 * "lld" on LLP64/ILP32), and the 8/16-bit builtins already carry the hh/h
 * length modifier so the same macro is correct for both printf and scanf. */
#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>
#include <c2go.h>

/* wcstoimax/wcstoumax take a const wchar_t* + wchar_t** — pull in wchar_t. */
#define __NEED_wchar_t
#include <bits/alltypes.h>

/* ── printf (PRI) ── */
#define PRId8  __INT8_FMTd__
#define PRIi8  __INT8_FMTi__
#define PRIo8  __UINT8_FMTo__
#define PRIu8  __UINT8_FMTu__
#define PRIx8  __UINT8_FMTx__
#define PRIX8  __UINT8_FMTX__
#define PRId16 __INT16_FMTd__
#define PRIi16 __INT16_FMTi__
#define PRIo16 __UINT16_FMTo__
#define PRIu16 __UINT16_FMTu__
#define PRIx16 __UINT16_FMTx__
#define PRIX16 __UINT16_FMTX__
#define PRId32 __INT32_FMTd__
#define PRIi32 __INT32_FMTi__
#define PRIo32 __UINT32_FMTo__
#define PRIu32 __UINT32_FMTu__
#define PRIx32 __UINT32_FMTx__
#define PRIX32 __UINT32_FMTX__
#define PRId64 __INT64_FMTd__
#define PRIi64 __INT64_FMTi__
#define PRIo64 __UINT64_FMTo__
#define PRIu64 __UINT64_FMTu__
#define PRIx64 __UINT64_FMTx__
#define PRIX64 __UINT64_FMTX__
#define PRIdMAX __INTMAX_FMTd__
#define PRIiMAX __INTMAX_FMTi__
#define PRIoMAX __UINTMAX_FMTo__
#define PRIuMAX __UINTMAX_FMTu__
#define PRIxMAX __UINTMAX_FMTx__
#define PRIXMAX __UINTMAX_FMTX__
#define PRIdPTR __INTPTR_FMTd__
#define PRIiPTR __INTPTR_FMTi__
#define PRIoPTR __UINTPTR_FMTo__
#define PRIuPTR __UINTPTR_FMTu__
#define PRIxPTR __UINTPTR_FMTx__
#define PRIXPTR __UINTPTR_FMTX__

/* ── scanf (SCN) — same builtins (they carry hh/h for the narrow types) ── */
#define SCNd8  __INT8_FMTd__
#define SCNi8  __INT8_FMTi__
#define SCNo8  __UINT8_FMTo__
#define SCNu8  __UINT8_FMTu__
#define SCNx8  __UINT8_FMTx__
#define SCNd16 __INT16_FMTd__
#define SCNi16 __INT16_FMTi__
#define SCNo16 __UINT16_FMTo__
#define SCNu16 __UINT16_FMTu__
#define SCNx16 __UINT16_FMTx__
#define SCNd32 __INT32_FMTd__
#define SCNi32 __INT32_FMTi__
#define SCNo32 __UINT32_FMTo__
#define SCNu32 __UINT32_FMTu__
#define SCNx32 __UINT32_FMTx__
#define SCNd64 __INT64_FMTd__
#define SCNi64 __INT64_FMTi__
#define SCNo64 __UINT64_FMTo__
#define SCNu64 __UINT64_FMTu__
#define SCNx64 __UINT64_FMTx__
#define SCNdMAX __INTMAX_FMTd__
#define SCNiMAX __INTMAX_FMTi__
#define SCNoMAX __UINTMAX_FMTo__
#define SCNuMAX __UINTMAX_FMTu__
#define SCNxMAX __UINTMAX_FMTx__
#define SCNdPTR __INTPTR_FMTd__
#define SCNiPTR __INTPTR_FMTi__
#define SCNoPTR __UINTPTR_FMTo__
#define SCNuPTR __UINTPTR_FMTu__
#define SCNxPTR __UINTPTR_FMTx__

typedef struct { intmax_t quot, rem; } imaxdiv_t;

intmax_t  imaxabs(intmax_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.imaxabs", C2GO_GOABI0);
imaxdiv_t imaxdiv(intmax_t, intmax_t)
    c2go_returntype(imaxdiv_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.imaxdiv", C2GO_GOABI0);
intmax_t  strtoimax(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtoimax", C2GO_GOABI0);
uintmax_t strtoumax(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtoumax", C2GO_GOABI0);

/* wide-string greatest-width twins (source/stdio.c) == wcstoll / wcstoull. */
intmax_t  wcstoimax(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstoimax", C2GO_GOABI0);
uintmax_t wcstoumax(const wchar_t *__restrict, wchar_t **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstoumax", C2GO_GOABI0);

#endif /* _INTTYPES_H */
