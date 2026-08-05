/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/dirent.h>
#include <dirent.h> /* namespaced managed and root unmanaged APIs coexist */
#if !defined(_WIN32)
#include <unistd.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_dir_test_gc(void);

static int is_name(const struct dirent *entry, const char *name)
{
    unsigned i = 0;
    while (entry->d_name[i] && entry->d_name[i] == name[i]) ++i;
    return entry->d_name[i] == name[i];
}

c2go_extern int mlib_dirent_prefixed_selftest(void)
{
    mlib_DIR *dir = mlib_opendir(".");
    struct dirent buffer;
    struct dirent *result = (void *)0;
    struct dirent *entry;
    if (!dir) return 1;

    /* opendir allocated a typed managed DIR. Its embedded state pointer must
     * survive collection without any handle-table root. */
    c2go_mlib_dir_test_gc();
    entry = mlib_readdir(dir);
    if (!entry || !is_name(entry, ".")) return 2;
    entry = mlib_readdir(dir);
    if (!entry || !is_name(entry, "..")) return 3;

    mlib_rewinddir(dir);
    c2go_mlib_dir_test_gc();
    if (mlib_readdir_r(dir, &buffer, &result) != 0) return 4;
    if (result != &buffer || !is_name(result, ".")) return 5;

#if !defined(_WIN32)
    {
        int copy = dup(mlib_dirfd(dir));
        mlib_DIR *from_fd;
        if (copy < 0) return 6;
        from_fd = mlib_fdopendir(copy);
        if (!from_fd) {
            close(copy);
            return 7;
        }
        c2go_mlib_dir_test_gc();
        entry = mlib_readdir(from_fd);
        if (!entry || !is_name(entry, ".")) return 8;
        if (mlib_closedir(from_fd) != 0) return 9;
    }
#endif

    return mlib_closedir(dir) == 0 ? 0 : 10;
}

#pragma c2go pop
