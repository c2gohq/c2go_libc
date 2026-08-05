/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_GLOB_TYPE
#error "C2GO_MLIB_TEST_GLOB_TYPE must name the managed glob result type"
#endif
#ifndef C2GO_MLIB_TEST_GLOB
#error "C2GO_MLIB_TEST_GLOB must name the managed glob function"
#endif
#ifndef C2GO_MLIB_TEST_GLOBFREE
#error "C2GO_MLIB_TEST_GLOBFREE must name the managed globfree function"
#endif
#ifndef C2GO_MLIB_TEST_GLOB_EXPORT
#error "C2GO_MLIB_TEST_GLOB_EXPORT must name the exported selftest"
#endif
#ifndef C2GO_MLIB_TEST_GLOB_FORCE_GC
#error "C2GO_MLIB_TEST_GLOB_FORCE_GC must trigger a Go collection"
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int c2go_mlib_glob_path(char *out, size_t capacity,
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

static int c2go_mlib_glob_mkfile(const char *base, const char *relative)
{
    char path[600];
    int fd;

    if (c2go_mlib_glob_path(path, sizeof(path), base, relative)) return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    return close(fd);
}

c2go_extern int C2GO_MLIB_TEST_GLOB_EXPORT(void)
{
    C2GO_MLIB_TEST_GLOB_TYPE result = {0};
    const char *tmp = getenv("TMPDIR");
    char root[512], pattern[600], expected_a[600], expected_b[600];
    char expected_c[600], expected_sub[600], marked_sub[602];
    char **managed paths;
    size_t i;
    int found_sub = 0;

    if (!tmp || !tmp[0]) tmp = getenv("TEMP");
    if (!tmp || !tmp[0]) tmp = ".";
    if (c2go_mlib_glob_path(root, sizeof(root), tmp,
                            "c2go_mlib_glob_XXXXXX"))
        return 1;
    if (!mkdtemp(root)) return 1;
    if (c2go_mlib_glob_mkfile(root, "b.c")) return 2;
    if (c2go_mlib_glob_mkfile(root, "a.c")) return 3;
    if (c2go_mlib_glob_mkfile(root, "c.h")) return 4;
    if (c2go_mlib_glob_path(expected_sub, sizeof(expected_sub), root, "sub"))
        return 5;
    if (mkdir(expected_sub, 0755)) return 5;
    if (c2go_mlib_glob_path(expected_a, sizeof(expected_a), root, "a.c") ||
        c2go_mlib_glob_path(expected_b, sizeof(expected_b), root, "b.c") ||
        c2go_mlib_glob_path(expected_c, sizeof(expected_c), root, "c.h") ||
        c2go_mlib_glob_path(pattern, sizeof(pattern), root, "*.c"))
        return 6;

    if (C2GO_MLIB_TEST_GLOB(pattern, 0, (void *)0, &result) != 0)
        return 7;
    if (result.gl_pathc != 2 || !result.gl_pathv) return 8;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();
    paths = result.gl_pathv;
    if (strcmp(paths[0], expected_a) || strcmp(paths[1], expected_b) || paths[2])
        return 9;

    if (c2go_mlib_glob_path(pattern, sizeof(pattern), root, "*.h")) return 10;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();
    if (C2GO_MLIB_TEST_GLOB(pattern, GLOB_APPEND, (void *)0, &result) != 0)
        return 10;
    if (result.gl_pathc != 3 || !result.gl_pathv) return 11;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();
    paths = result.gl_pathv;
    if (strcmp(paths[0], expected_a) || strcmp(paths[1], expected_b) ||
        strcmp(paths[2], expected_c) || paths[3])
        return 12;
    C2GO_MLIB_TEST_GLOBFREE(&result);
    if (result.gl_pathc || result.gl_pathv) return 13;
    paths = (void *)0;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();

    result.gl_offs = 2;
    if (c2go_mlib_glob_path(pattern, sizeof(pattern), root, "*.c")) return 14;
    if (C2GO_MLIB_TEST_GLOB(pattern, GLOB_DOOFFS, (void *)0, &result) != 0)
        return 14;
    paths = result.gl_pathv;
    if (result.gl_pathc != 2 || !paths || paths[0] || paths[1] ||
        strcmp(paths[2], expected_a) || strcmp(paths[3], expected_b) || paths[4])
        return 15;
    if (c2go_mlib_glob_path(pattern, sizeof(pattern), root, "*.h")) return 16;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();
    if (C2GO_MLIB_TEST_GLOB(pattern, GLOB_DOOFFS | GLOB_APPEND,
                            (void *)0, &result) != 0)
        return 16;
    paths = result.gl_pathv;
    if (result.gl_pathc != 3 || !paths || paths[0] || paths[1] ||
        strcmp(paths[2], expected_a) || strcmp(paths[3], expected_b) ||
        strcmp(paths[4], expected_c) || paths[5])
        return 17;
    C2GO_MLIB_TEST_GLOBFREE(&result);
    paths = (void *)0;

    if (c2go_mlib_glob_path(pattern, sizeof(pattern), root, "missing-*.zz"))
        return 18;
    if (C2GO_MLIB_TEST_GLOB(pattern, GLOB_NOCHECK, (void *)0, &result) != 0)
        return 18;
    if (result.gl_pathc != 1 || strcmp(result.gl_pathv[0], pattern) ||
        result.gl_pathv[1])
        return 19;
    C2GO_MLIB_TEST_GLOBFREE(&result);
    if (C2GO_MLIB_TEST_GLOB(pattern, 0, (void *)0, &result) != GLOB_NOMATCH)
        return 20;
    if (result.gl_pathc || result.gl_pathv) return 21;

    if (c2go_mlib_glob_path(pattern, sizeof(pattern), root, "*")) return 22;
    if (strlen(expected_sub) + 2 > sizeof(marked_sub)) return 22;
    memcpy(marked_sub, expected_sub, strlen(expected_sub));
    marked_sub[strlen(expected_sub)] = '/';
    marked_sub[strlen(expected_sub) + 1] = 0;
    if (C2GO_MLIB_TEST_GLOB(pattern, GLOB_MARK, (void *)0, &result) != 0)
        return 22;
    C2GO_MLIB_TEST_GLOB_FORCE_GC();
    for (i = 0; i < result.gl_pathc; ++i)
        if (!strcmp(result.gl_pathv[i], marked_sub)) found_sub = 1;
    if (!found_sub) return 23;
    C2GO_MLIB_TEST_GLOBFREE(&result);

    if (unlink(expected_a) || unlink(expected_b) || unlink(expected_c) ||
        rmdir(expected_sub) || rmdir(root))
        return 24;
    return 0;
}

#undef C2GO_MLIB_TEST_GLOB_FORCE_GC
#undef C2GO_MLIB_TEST_GLOB_EXPORT
#undef C2GO_MLIB_TEST_GLOBFREE
#undef C2GO_MLIB_TEST_GLOB
#undef C2GO_MLIB_TEST_GLOB_TYPE
