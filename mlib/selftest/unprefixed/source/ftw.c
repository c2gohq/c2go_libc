/* SPDX-License-Identifier: AGPL-3.0-only */

#if !defined(_WIN32)

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/ftw.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_ftw_test_gc(void);

#define C2GO_MLIB_TEST_NFTW nftw
#define C2GO_MLIB_TEST_FTW ftw
#define C2GO_MLIB_TEST_EXPORT mlib_ftw_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_ftw_test_gc()
#include "../../source/ftw_fixture.inc"

#pragma c2go pop

#endif /* !_WIN32 */
