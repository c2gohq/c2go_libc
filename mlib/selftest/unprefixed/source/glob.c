/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/glob.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_glob_test_gc(void);

#define C2GO_MLIB_TEST_GLOB_TYPE glob_t
#define C2GO_MLIB_TEST_GLOB glob
#define C2GO_MLIB_TEST_GLOBFREE globfree
#define C2GO_MLIB_TEST_GLOB_EXPORT mlib_glob_unprefixed_selftest
#define C2GO_MLIB_TEST_GLOB_FORCE_GC() c2go_mlib_glob_test_gc()
#include "../../source/glob_fixture.inc"

#pragma c2go pop
