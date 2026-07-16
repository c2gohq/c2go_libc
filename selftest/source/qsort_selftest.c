/* qsort_selftest.c — in-C exercise of qsort + bsearch, driven from Go via
 * QsortSelftest(). qsort/bsearch take a comparator that is CALLED FROM C, so
 * they cannot be driven with a Go closure directly; this self-test provides a C
 * comparator and checks the results across several sizes and input patterns
 * (pseudo-random via an LCG, reverse-sorted, all-equal, already-sorted) plus a
 * bsearch hit/miss sweep. It lives in its own translation unit, so it also
 * exercises the cross-TU c2go_linkname call into qsort/bsearch. Returns 0 on
 * success or a small non-zero code identifying the first failing check. */
#include <stdlib.h>

static int cmp_dir(const void *a, const void *b, void *arg)
{
    int d = *(const int *)arg;
    int x = *(const int *)a, y = *(const int *)b;
    return d * ((x > y) - (x < y));
}

static int cmp_i32(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int is_sorted(const int *a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

c2go_extern int QsortSelftest(void) {
    static int buf[1000];
    size_t sizes[] = {0, 1, 2, 3, 5, 8, 17, 64, 129, 500, 1000};
    unsigned seed = 12345u;

    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        size_t n = sizes[si];

        for (size_t i = 0; i < n; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = (int)((seed >> 8) % 1000u) - 500;
        }
        qsort(buf, n, sizeof(int), cmp_i32);
        if (!is_sorted(buf, n)) return 1;

        for (size_t i = 0; i < n; i++) buf[i] = (int)(n - i);
        qsort(buf, n, sizeof(int), cmp_i32);
        if (!is_sorted(buf, n)) return 2;

        for (size_t i = 0; i < n; i++) buf[i] = 7;
        qsort(buf, n, sizeof(int), cmp_i32);
        if (!is_sorted(buf, n)) return 3;

        for (size_t i = 0; i < n; i++) buf[i] = (int)i;
        qsort(buf, n, sizeof(int), cmp_i32);
        if (!is_sorted(buf, n)) return 4;
    }

    static int s[100];
    for (int i = 0; i < 100; i++) s[i] = i * 2; /* 0,2,4,...,198 */
    for (int i = 0; i < 100; i++) {
        int key = i * 2;
        int *p = bsearch(&key, s, 100, sizeof(int), cmp_i32);
        if (!p || *p != key) return 5;
    }
    int miss = 3; /* odd -> absent */
    if (bsearch(&miss, s, 100, sizeof(int), cmp_i32) != NULL) return 6;
    int lo = -1;
    if (bsearch(&lo, s, 100, sizeof(int), cmp_i32) != NULL) return 7;

    /* 8: qsort_r (#664) — the arg selects the direction; verifies the kernel
     * threads arg to every comparator call. */
    {
        int buf[64], dir = -1; /* descending */
        for (int i = 0; i < 64; i++) buf[i] = (i * 37) % 64;
        qsort_r(buf, 64, sizeof(int), cmp_dir, &dir);
        for (int i = 1; i < 64; i++)
            if (buf[i - 1] < buf[i]) return 8;
        dir = 1; /* ascending */
        qsort_r(buf, 64, sizeof(int), cmp_dir, &dir);
        for (int i = 1; i < 64; i++)
            if (buf[i - 1] > buf[i]) return 8;
    }

    return 0;
}
