/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_WSTRING_H
#define C2GO_MLIB_WSTRING_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* This header changes only the allocation-returning wide-string function. It
 * deliberately does not include <wchar.h> or select its FILE surface; users
 * that also need wide APIs include <c2go/mlib/wchar.h>. */
#define __NEED_wchar_t
#define __NEED_size_t
#include <bits/alltypes.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* The duplicate is GC-owned no-pointer storage. Never pass it to free(); set
 * the managed owner to NULL after its final use so it becomes unreachable. */
wchar_t *managed mlib_wcsdup(const wchar_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_wcsdup", C2GO_GOABI0);
#ifdef C2GO_MLIB_UNPREFIXED
/* If ordinary <wchar.h> is included later, keep its unmanaged wcsdup route out
 * of this translation unit while retaining all stateless wide functions. */
#define C2GO_WCHAR_OMIT_DUP 1
#define wcsdup mlib_wcsdup
#endif

#pragma c2go pop

#endif /* C2GO_MLIB_WSTRING_H */
