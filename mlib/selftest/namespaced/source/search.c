/* SPDX-License-Identifier: AGPL-3.0-only */

#define _GNU_SOURCE
#include <c2go/mlib/search.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_search_test_gc(void);

#define C2GO_MLIB_TEST_ENTRY mlib_ENTRY
#define C2GO_MLIB_TEST_HSEARCH_DATA struct mlib_hsearch_data
#define C2GO_MLIB_TEST_HCREATE mlib_hcreate
#define C2GO_MLIB_TEST_HDESTROY mlib_hdestroy
#define C2GO_MLIB_TEST_HSEARCH mlib_hsearch
#define C2GO_MLIB_TEST_HCREATE_R mlib_hcreate_r
#define C2GO_MLIB_TEST_HDESTROY_R mlib_hdestroy_r
#define C2GO_MLIB_TEST_HSEARCH_R mlib_hsearch_r
#define C2GO_MLIB_TEST_TSEARCH mlib_tsearch
#define C2GO_MLIB_TEST_TFIND mlib_tfind
#define C2GO_MLIB_TEST_TDELETE mlib_tdelete
#define C2GO_MLIB_TEST_TWALK mlib_twalk
#define C2GO_MLIB_TEST_TDESTROY mlib_tdestroy
#define C2GO_MLIB_TEST_INSQUE mlib_insque
#define C2GO_MLIB_TEST_REMQUE mlib_remque
#define C2GO_MLIB_TEST_EXPORT mlib_search_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_search_test_gc()
#include "../../source/search_fixture.inc"

#pragma c2go pop
