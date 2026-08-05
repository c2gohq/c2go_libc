/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/glob.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_glob_test_gc(void);

#define C2GO_MLIB_TEST_GLOB_TYPE mlib_glob_t
#define C2GO_MLIB_TEST_GLOB mlib_glob
#define C2GO_MLIB_TEST_GLOBFREE mlib_globfree
#define C2GO_MLIB_TEST_GLOB_EXPORT mlib_glob_prefixed_selftest
#define C2GO_MLIB_TEST_GLOB_FORCE_GC() c2go_mlib_glob_test_gc()
#include "../../source/glob_fixture.inc"

#pragma c2go pop
