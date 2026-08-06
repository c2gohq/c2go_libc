/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/dirent.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_dirent_retire_test_gc(void);

#define C2GO_MLIB_TEST_DIR DIR
#define C2GO_MLIB_TEST_OPENDIR opendir
#define C2GO_MLIB_TEST_CLOSEDIR closedir
#define C2GO_MLIB_TEST_SCANDIR scandir
#define C2GO_MLIB_TEST_EXPORT mlib_dirent_retire_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_dirent_retire_test_gc()
#include "../../source/dirent_retire_fixture.inc"

#pragma c2go pop
