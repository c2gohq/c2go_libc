/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/string.h>
#include <c2go/mlib/wstring.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_string_test_gc(void);

#define C2GO_MLIB_TEST_STRDUP strdup
#define C2GO_MLIB_TEST_STRNDUP strndup
#define C2GO_MLIB_TEST_WCSDUP wcsdup
#define C2GO_MLIB_TEST_EXPORT mlib_string_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_string_test_gc()
#include "../../source/string_fixture.inc"

#pragma c2go pop
