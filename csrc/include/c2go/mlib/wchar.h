/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_WCHAR_H
#define C2GO_MLIB_WCHAR_H

/* stdio replacement mode must be established before <wchar.h> decides
 * whether root FILE declarations are safe to expose. Namespaced mode keeps
 * both root and managed wide-stream families available. */
#include <c2go/mlib/stdio.h>
#include <wchar.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

int C2GO_MLIB_NAME(fwide)(C2GO_MLIB_NAME(FILE) *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fwide", C2GO_GOABI0);

wint_t C2GO_MLIB_NAME(fputwc)(wchar_t, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fputwc", C2GO_GOABI0);
wint_t C2GO_MLIB_NAME(putwc)(wchar_t, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_putwc", C2GO_GOABI0);
wint_t C2GO_MLIB_NAME(putwchar)(wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_putwchar", C2GO_GOABI0);

wint_t C2GO_MLIB_NAME(fgetwc)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fgetwc", C2GO_GOABI0);
wint_t C2GO_MLIB_NAME(getwc)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_getwc", C2GO_GOABI0);
wint_t C2GO_MLIB_NAME(getwchar)(void)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_getwchar", C2GO_GOABI0);

int C2GO_MLIB_NAME(fputws)(const wchar_t *__restrict,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fputws", C2GO_GOABI0);
wchar_t *C2GO_MLIB_NAME(fgetws)(wchar_t *__restrict, int,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fgetws", C2GO_GOABI0);
wint_t C2GO_MLIB_NAME(ungetwc)(wint_t, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ungetwc", C2GO_GOABI0);

int C2GO_MLIB_NAME(vfwprintf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vfwprintf", C2GO_GOABI0);
int C2GO_MLIB_NAME(fwprintf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fwprintf", C2GO_GOABI0);
int C2GO_MLIB_NAME(vwprintf)(const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vwprintf", C2GO_GOABI0);
int C2GO_MLIB_NAME(wprintf)(const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_wprintf", C2GO_GOABI0);

/* `%m` result buffers are GC-owned, and `%m`/`%p` pointer results are
 * published through managed write barriers. Do not pass `%m` results to
 * free(); non-null `%p` results must denote valid Go-managed addresses. */
int C2GO_MLIB_NAME(vfwscanf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vfwscanf", C2GO_GOABI0);
int C2GO_MLIB_NAME(fwscanf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fwscanf", C2GO_GOABI0);
int C2GO_MLIB_NAME(vwscanf)(const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vwscanf", C2GO_GOABI0);
int C2GO_MLIB_NAME(wscanf)(const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_wscanf", C2GO_GOABI0);
int C2GO_MLIB_NAME(vswscanf)(const wchar_t *__restrict,
    const wchar_t *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vswscanf", C2GO_GOABI0);
int C2GO_MLIB_NAME(swscanf)(const wchar_t *__restrict,
    const wchar_t *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_swscanf", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_WCHAR_H */
