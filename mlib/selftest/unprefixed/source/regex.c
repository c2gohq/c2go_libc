/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/regex.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_regex_test_gc(void);

#define C2GO_MLIB_TEST_REGEX_T regex_t
#define C2GO_MLIB_TEST_REGCOMP regcomp
#define C2GO_MLIB_TEST_REGEXEC regexec
#define C2GO_MLIB_TEST_REGFREE regfree
#define C2GO_MLIB_TEST_EXPORT mlib_regex_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_regex_test_gc()
#include "../../source/regex_fixture.inc"

#pragma c2go pop
