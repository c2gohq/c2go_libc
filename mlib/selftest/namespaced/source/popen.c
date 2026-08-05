/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/stdio.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_popen_test_gc(void);

#define C2GO_MLIB_TEST_FILE mlib_FILE
#define C2GO_MLIB_TEST_POPEN mlib_popen
#define C2GO_MLIB_TEST_PCLOSE mlib_pclose
#define C2GO_MLIB_TEST_FGETS mlib_fgets
#define C2GO_MLIB_TEST_FPUTS mlib_fputs
#define C2GO_MLIB_TEST_FMEMOPEN mlib_fmemopen
#define C2GO_MLIB_TEST_FCLOSE mlib_fclose
#define C2GO_MLIB_TEST_EXPORT mlib_popen_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_popen_test_gc()
#include "../../source/popen_fixture.inc"

#pragma c2go pop
