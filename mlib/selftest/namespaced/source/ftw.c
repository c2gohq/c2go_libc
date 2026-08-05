/* SPDX-License-Identifier: AGPL-3.0-only */

#if !defined(_WIN32)

#include <c2go/mlib/ftw.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_ftw_test_gc(void);

#define C2GO_MLIB_TEST_NFTW mlib_nftw
#define C2GO_MLIB_TEST_FTW mlib_ftw
#define C2GO_MLIB_TEST_EXPORT mlib_ftw_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_ftw_test_gc()
#include "../../source/ftw_fixture.inc"

#pragma c2go pop

#endif /* !_WIN32 */
