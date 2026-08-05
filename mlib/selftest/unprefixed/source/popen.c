/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/stdio.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_popen_test_gc(void);

#define C2GO_MLIB_TEST_FILE FILE
#define C2GO_MLIB_TEST_POPEN popen
#define C2GO_MLIB_TEST_PCLOSE pclose
#define C2GO_MLIB_TEST_FGETS fgets
#define C2GO_MLIB_TEST_FPUTS fputs
#define C2GO_MLIB_TEST_FMEMOPEN fmemopen
#define C2GO_MLIB_TEST_FCLOSE fclose
#define C2GO_MLIB_TEST_EXPORT mlib_popen_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_popen_test_gc()
#include "../../source/popen_fixture.inc"

#pragma c2go pop
