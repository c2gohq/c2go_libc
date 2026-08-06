/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/stdlib.h>
#include <limits.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_realpath_test_gc(void);

#define C2GO_MLIB_TEST_REALPATH mlib_realpath
#define C2GO_MLIB_TEST_EXPORT mlib_realpath_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_realpath_test_gc()
#include "../../source/realpath_fixture.inc"

#pragma c2go pop
