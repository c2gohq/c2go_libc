/* nftw_selftest.c — in-C exercise of the #675 C-wave-2b <ftw.h> walk. The
 * fn callback is invoked FROM C, so the driver lives here (qsort_selftest
 * precedent). Builds a small fixed tree under $TMPDIR, walks it in
 * pre-order and FTW_DEPTH post-order, checks type/level accounting and
 * early-stop, then removes the tree with an FTW_DEPTH walk (dogfood).
 * readdir order is not portable, so the checks count per-type visits and
 * pin only the root's position. Returns 0 on success, a distinct code per
 * failing step. */
#if !defined(_WIN32)

#include <ftw.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <c2go.h>

static char root[512];

static int mkfile(const char *rel) {
    char p[600];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    int fd = open(p, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (write(fd, "x", 1) != 1) { close(fd); return -1; }
    return close(fd);
}

static int mkdirrel(const char *rel) {
    char p[600];
    if (!rel[0]) return mkdir(root, 0755);
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return mkdir(p, 0755);
}

static int n_d, n_f, n_dp, n_other, n_total, root_pos, max_level;

static int count_cb(const char *path, const struct stat *st, int type, struct FTW *lev) {
    (void)st;
    n_total++;
    if (!strcmp(path, root)) root_pos = n_total;
    if (type == FTW_D) n_d++;
    else if (type == FTW_F) n_f++;
    else if (type == FTW_DP) n_dp++;
    else n_other++;
    if (lev->level > max_level) max_level = lev->level;
    return 0;
}

static int stop_cb(const char *path, const struct stat *st, int type, struct FTW *lev) {
    (void)path; (void)st; (void)type; (void)lev;
    return ++n_total == 2 ? 7 : 0;
}

static int rm_cb(const char *path, const struct stat *st, int type, struct FTW *lev) {
    (void)st; (void)lev;
    return (type == FTW_DP ? rmdir(path) : unlink(path)) ? -1 : 0;
}

c2go_extern int nftw_selftest(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    snprintf(root, sizeof root, "%s/c2go_nftw_selftest", tmp);

    /* stale tree from a crashed run: remove via the same FTW_DEPTH walk */
    nftw(root, rm_cb, 8, FTW_DEPTH | FTW_PHYS);

    if (mkdirrel("")) return 1;
    if (mkdirrel("a")) return 2;
    if (mkdirrel("a/b")) return 3;
    if (mkfile("f1")) return 4;
    if (mkfile("a/f2")) return 5;
    if (mkfile("a/b/f3")) return 6;

    /* pre-order: root first; 3 dirs as FTW_D, 3 files, depth reaches 2+1 */
    n_d = n_f = n_dp = n_other = n_total = root_pos = max_level = 0;
    if (nftw(root, count_cb, 8, 0)) return 7;
    if (n_total != 6 || root_pos != 1) return 8;
    if (n_d != 3 || n_f != 3 || n_dp != 0 || n_other != 0) return 9;
    if (max_level != 3) return 10; /* root=0, a=1, b=2, f3=3 */

    /* FTW_DEPTH: dirs report FTW_DP and root comes LAST */
    n_d = n_f = n_dp = n_other = n_total = root_pos = max_level = 0;
    if (nftw(root, count_cb, 8, FTW_DEPTH)) return 11;
    if (n_total != 6 || root_pos != 6) return 12;
    if (n_dp != 3 || n_f != 3 || n_d != 0) return 13;

    /* nonzero fn return stops the walk and becomes nftw's return */
    n_total = 0;
    if (nftw(root, stop_cb, 8, 0) != 7) return 14;
    if (n_total != 2) return 15;

    /* fd_limit <= 0 is a no-op success (musl) */
    if (nftw(root, count_cb, 0, 0) != 0) return 16;

    /* missing path reports -1/ENOENT */
    errno = 0;
    if (nftw("/nonexistent/c2go/nftw", count_cb, 8, 0) != -1 || errno != ENOENT)
        return 17;

    /* remove the tree depth-first, then verify it is gone */
    if (nftw(root, rm_cb, 8, FTW_DEPTH | FTW_PHYS)) return 18;
    struct stat st;
    if (stat(root, &st) != -1 || errno != ENOENT) return 19;
    return 0;
}

#endif /* !_WIN32 */
