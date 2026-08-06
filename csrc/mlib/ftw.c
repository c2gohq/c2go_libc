/* SPDX-License-Identifier: AGPL-3.0-only
 *
 * Selective managed instantiation wrapper. The included musl source remains
 * available under musl's MIT license; see musl/COPYRIGHT. */

#if !defined(_WIN32)

/* Pre-include every dependency before applying the narrow source-renaming
 * macros. The musl files' own includes then resolve through their guards. */
#include <c2go/mlib/dirent.h>
#include <c2go/mlib/ftw.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <c2go.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* The pinned musl traversal takes DIR * by value. Keep that source unchanged,
 * but make the mlib instantiation retire its local owning slot immediately
 * after closedir invalidates the managed carrier. */
static int mlib_nftw_closedir(mlib_DIR **slot)
{
    int result = mlib_closedir(*slot);
    *slot = (void *)0;
    return result;
}

#define DIR       mlib_DIR
#define fdopendir mlib_fdopendir
#define readdir   mlib_readdir
#define closedir(d) mlib_nftw_closedir(&(d))
#define dirfd     mlib_dirfd
#define nftw      mlib_nftw
#define ftw       mlib_ftw

#include "../../musl/src/misc/nftw.c"
#include "../../musl/src/legacy/ftw.c"

#undef ftw
#undef nftw
#undef dirfd
#undef closedir
#undef readdir
#undef fdopendir
#undef DIR

#pragma c2go pop

#endif /* !_WIN32 */
