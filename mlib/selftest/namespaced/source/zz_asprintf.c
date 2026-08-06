/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/stdio.h>
#include <string.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_asprintf_test_gc(void);

#define C2GO_MLIB_TEST_ASPRINTF mlib_asprintf
#define C2GO_MLIB_TEST_VASPRINTF mlib_vasprintf
#define C2GO_MLIB_TEST_EXPORT mlib_asprintf_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_asprintf_test_gc()
#include "../../source/asprintf_fixture.inc"

#pragma c2go pop
