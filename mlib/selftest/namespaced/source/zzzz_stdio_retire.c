/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/stdio.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_stdio_retire_test_gc(void);

#define C2GO_MLIB_TEST_FILE mlib_FILE
#define C2GO_MLIB_TEST_FCLOSE mlib_fclose
#define C2GO_MLIB_TEST_STDERR mlib_stderr
#define C2GO_MLIB_TEST_EXPORT mlib_stdio_retire_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_stdio_retire_test_gc()
#include "../../source/stdio_retire_fixture.inc"

#pragma c2go pop
