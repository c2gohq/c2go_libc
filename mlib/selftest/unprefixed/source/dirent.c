/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/dirent.h>
#include <dirent.h> /* claimed guard keeps the unmanaged DIR ABI out */
#if !defined(_WIN32)
#include <unistd.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_dir_test_gc(void);

static int is_name(const struct dirent *entry, const char *name)
{
    unsigned i = 0;
    while (entry->d_name[i] && entry->d_name[i] == name[i]) ++i;
    return entry->d_name[i] == name[i];
}

static int select_dots(const struct dirent *entry)
{
    return is_name(entry, ".") || is_name(entry, "..");
}

static int force_gc_in_compare;

static int compare_reverse_alpha(const struct dirent **a,
                                 const struct dirent **b)
{
    if (force_gc_in_compare) {
        force_gc_in_compare = 0;
        c2go_mlib_dir_test_gc();
    }
    return alphasort(b, a);
}

c2go_extern int mlib_dirent_unprefixed_selftest(void)
{
    DIR *dir = opendir(".");
    struct dirent buffer;
    struct dirent *result = (void *)0;
    struct dirent *entry;
    if (!dir) return 1;

    c2go_mlib_dir_test_gc();
    entry = readdir(dir);
    if (!entry || !is_name(entry, ".")) return 2;
    entry = readdir(dir);
    if (!entry || !is_name(entry, "..")) return 3;

    rewinddir(dir);
    c2go_mlib_dir_test_gc();
    if (readdir_r(dir, &buffer, &result) != 0) return 4;
    if (result != &buffer || !is_name(result, ".")) return 5;

#if !defined(_WIN32)
    {
        int copy = dup(dirfd(dir));
        DIR *from_fd;
        if (copy < 0) return 6;
        from_fd = fdopendir(copy);
        if (!from_fd) {
            close(copy);
            return 7;
        }
        c2go_mlib_dir_test_gc();
        entry = readdir(from_fd);
        if (!entry || !is_name(entry, ".")) return 8;
        if (closedir(from_fd) != 0) return 9;
    }
#endif

    if (closedir(dir) != 0) return 10;

    {
        struct dirent **names = (void *)0;
        force_gc_in_compare = 1;
        int count = scandir(".", &names, (void *)0, compare_reverse_alpha);
        int found_dot = 0, found_dotdot = 0;
        if (count < 2) return 11;
        c2go_mlib_dir_test_gc();
        for (int i = 0; i < count; ++i) {
            if (is_name(names[i], ".")) found_dot = 1;
            if (is_name(names[i], "..")) found_dotdot = 1;
            if (i && compare_reverse_alpha(
                         (const struct dirent **)&names[i - 1],
                         (const struct dirent **)&names[i]) > 0)
                return 12;
        }
        if (!found_dot || !found_dotdot) return 13;

        names = (void *)0;
        count = scandir(".", &names, select_dots, compare_reverse_alpha);
        if (count != 2) return 14;
        c2go_mlib_dir_test_gc();
        if (!is_name(names[0], "..") || !is_name(names[1], ".")) return 15;
    }

    return 0;
}

#pragma c2go pop
