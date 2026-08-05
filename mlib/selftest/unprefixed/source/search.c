/* SPDX-License-Identifier: AGPL-3.0-only */

#define _GNU_SOURCE
#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/search.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_search_test_gc(void);

#define C2GO_MLIB_TEST_ENTRY ENTRY
#define C2GO_MLIB_TEST_HSEARCH_DATA struct hsearch_data
#define C2GO_MLIB_TEST_HCREATE hcreate
#define C2GO_MLIB_TEST_HDESTROY hdestroy
#define C2GO_MLIB_TEST_HSEARCH hsearch
#define C2GO_MLIB_TEST_HCREATE_R hcreate_r
#define C2GO_MLIB_TEST_HDESTROY_R hdestroy_r
#define C2GO_MLIB_TEST_HSEARCH_R hsearch_r
#define C2GO_MLIB_TEST_TSEARCH tsearch
#define C2GO_MLIB_TEST_TFIND tfind
#define C2GO_MLIB_TEST_TDELETE tdelete
#define C2GO_MLIB_TEST_TWALK twalk
#define C2GO_MLIB_TEST_TDESTROY tdestroy
#define C2GO_MLIB_TEST_INSQUE insque
#define C2GO_MLIB_TEST_REMQUE remque
#define C2GO_MLIB_TEST_EXPORT mlib_search_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_search_test_gc()
#include "../../source/search_fixture.inc"

#pragma c2go pop
