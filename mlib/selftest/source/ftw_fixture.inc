/* SPDX-License-Identifier: AGPL-3.0-only */

/* Shared managed ftw/nftw fixture. The including translation unit supplies
 * namespace-specific API and export names while this file keeps both modes on
 * exactly the same behavioral and GC-safety test. */
#ifndef C2GO_MLIB_TEST_NFTW
#error "C2GO_MLIB_TEST_NFTW must name the managed nftw function"
#endif
#ifndef C2GO_MLIB_TEST_FTW
#error "C2GO_MLIB_TEST_FTW must name the managed ftw function"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the exported selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char c2go_mlib_ftw_root[512];
static int c2go_mlib_ftw_dirs;
static int c2go_mlib_ftw_files;
static int c2go_mlib_ftw_depth_dirs;
static int c2go_mlib_ftw_other;
static int c2go_mlib_ftw_total;
static int c2go_mlib_ftw_root_pos;
static int c2go_mlib_ftw_max_level;
static int c2go_mlib_ftw_force_gc;

static int c2go_mlib_ftw_path(char *out, size_t capacity,
                              const char *base, const char *relative)
{
    size_t base_length = strlen(base);
    size_t relative_length = strlen(relative);

    if (base_length > capacity || relative_length > capacity - base_length ||
        base_length + relative_length + 2 > capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(out, base, base_length);
    out[base_length] = '/';
    memcpy(out + base_length + 1, relative, relative_length + 1);
    return 0;
}

static int c2go_mlib_ftw_mkfile(const char *relative)
{
    char path[600];
    int fd;

    if (c2go_mlib_ftw_path(path, sizeof(path), c2go_mlib_ftw_root,
                           relative))
        return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (write(fd, "x", 1) != 1) {
        close(fd);
        return -1;
    }
    return close(fd);
}

static int c2go_mlib_ftw_mkdir(const char *relative)
{
    char path[600];

    if (c2go_mlib_ftw_path(path, sizeof(path), c2go_mlib_ftw_root,
                           relative))
        return -1;
    return mkdir(path, 0755);
}

static void c2go_mlib_ftw_maybe_gc(void)
{
    if (!c2go_mlib_ftw_force_gc) return;
    c2go_mlib_ftw_force_gc = 0;
    C2GO_MLIB_TEST_FORCE_GC();
}

static int c2go_mlib_nftw_count(const char *path, const struct stat *st,
                                int type, struct FTW *level)
{
    (void)st;
    c2go_mlib_ftw_maybe_gc();
    ++c2go_mlib_ftw_total;
    if (!strcmp(path, c2go_mlib_ftw_root))
        c2go_mlib_ftw_root_pos = c2go_mlib_ftw_total;
    if (type == FTW_D) ++c2go_mlib_ftw_dirs;
    else if (type == FTW_F) ++c2go_mlib_ftw_files;
    else if (type == FTW_DP) ++c2go_mlib_ftw_depth_dirs;
    else ++c2go_mlib_ftw_other;
    if (level->level > c2go_mlib_ftw_max_level)
        c2go_mlib_ftw_max_level = level->level;
    return 0;
}

static int c2go_mlib_ftw_count(const char *path, const struct stat *st,
                               int type)
{
    (void)path;
    (void)st;
    (void)type;
    c2go_mlib_ftw_maybe_gc();
    ++c2go_mlib_ftw_total;
    return 0;
}

static int c2go_mlib_nftw_stop(const char *path, const struct stat *st,
                               int type, struct FTW *level)
{
    (void)path;
    (void)st;
    (void)type;
    (void)level;
    return ++c2go_mlib_ftw_total == 2 ? 7 : 0;
}

static int c2go_mlib_nftw_remove(const char *path, const struct stat *st,
                                 int type, struct FTW *level)
{
    (void)st;
    (void)level;
    return (type == FTW_DP ? rmdir(path) : unlink(path)) ? -1 : 0;
}

static void c2go_mlib_ftw_reset_counts(void)
{
    c2go_mlib_ftw_dirs = 0;
    c2go_mlib_ftw_files = 0;
    c2go_mlib_ftw_depth_dirs = 0;
    c2go_mlib_ftw_other = 0;
    c2go_mlib_ftw_total = 0;
    c2go_mlib_ftw_root_pos = 0;
    c2go_mlib_ftw_max_level = 0;
}

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    const char *tmp = getenv("TMPDIR");
    struct stat st;

    if (!tmp || !tmp[0]) tmp = "/tmp";
    if (c2go_mlib_ftw_path(c2go_mlib_ftw_root,
                           sizeof(c2go_mlib_ftw_root), tmp,
                           "c2go_mlib_ftw_XXXXXX"))
        return 1;
    if (!mkdtemp(c2go_mlib_ftw_root)) return 1;
    if (c2go_mlib_ftw_mkdir("a")) return 2;
    if (c2go_mlib_ftw_mkdir("a/b")) return 3;
    if (c2go_mlib_ftw_mkfile("f1")) return 4;
    if (c2go_mlib_ftw_mkfile("a/f2")) return 5;
    if (c2go_mlib_ftw_mkfile("a/b/f3")) return 6;

    c2go_mlib_ftw_reset_counts();
    c2go_mlib_ftw_force_gc = 1;
    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_count,
                            8, 0))
        return 7;
    if (c2go_mlib_ftw_total != 6 || c2go_mlib_ftw_root_pos != 1)
        return 8;
    if (c2go_mlib_ftw_dirs != 3 || c2go_mlib_ftw_files != 3 ||
        c2go_mlib_ftw_depth_dirs != 0 || c2go_mlib_ftw_other != 0)
        return 9;
    if (c2go_mlib_ftw_max_level != 3) return 10;

    c2go_mlib_ftw_reset_counts();
    c2go_mlib_ftw_force_gc = 1;
    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_count,
                            8, FTW_DEPTH))
        return 11;
    if (c2go_mlib_ftw_total != 6 || c2go_mlib_ftw_root_pos != 6)
        return 12;
    if (c2go_mlib_ftw_depth_dirs != 3 || c2go_mlib_ftw_files != 3 ||
        c2go_mlib_ftw_dirs != 0 || c2go_mlib_ftw_other != 0)
        return 13;

    c2go_mlib_ftw_total = 0;
    c2go_mlib_ftw_force_gc = 1;
    if (C2GO_MLIB_TEST_FTW(c2go_mlib_ftw_root, c2go_mlib_ftw_count, 8))
        return 14;
    if (c2go_mlib_ftw_total != 6) return 15;

    c2go_mlib_ftw_total = 0;
    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_stop,
                            8, 0) != 7)
        return 16;
    if (c2go_mlib_ftw_total != 2) return 17;
    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_count,
                            0, 0) != 0)
        return 18;

    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_remove,
                            8, FTW_DEPTH | FTW_PHYS))
        return 19;
    errno = 0;
    if (stat(c2go_mlib_ftw_root, &st) != -1 || errno != ENOENT) return 20;
    errno = 0;
    if (C2GO_MLIB_TEST_NFTW(c2go_mlib_ftw_root, c2go_mlib_nftw_count,
                            8, 0) != -1 || errno != ENOENT)
        return 21;
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_FTW
#undef C2GO_MLIB_TEST_NFTW
