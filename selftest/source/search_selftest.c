/* search_selftest.c — in-C exercise of the #668 <search.h> family. The
 * comparators/callbacks are invoked FROM C (tsearch/tdelete/lsearch), so the
 * driver lives here (qsort_selftest precedent). Returns 0 on success, a
 * distinct code per failing step. The full semantic surface is probe_search
 * (dual-arch dual-O vs the host BSD oracle + musl-pinned GNU section); this
 * is the in-tree regression face. */
#define _GNU_SOURCE
#include <search.h>
#include <string.h>
#include <c2go.h>

static int icmp(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int keys[5] = {5, 3, 8, 1, 6};
static int freek_calls;
static void freek(void *k) { (void)k; freek_calls++; }

static char walk_buf[64];
static void inorder_cb(const void *nodep, VISIT v, int d) {
    (void)d;
    if (v == postorder || v == leaf) {
        char t[8];
        int k = **(int *const *)nodep, i = 0;
        do { t[i++] = (char)('0' + k % 10); k /= 10; } while (k);
        while (i) { size_t l = strlen(walk_buf); walk_buf[l] = t[--i]; walk_buf[l+1] = 0; }
    }
}

c2go_extern int search_selftest(void) {
    /* tsearch/tfind/tdelete/twalk/tdestroy */
    void *root = 0;
    int i;
    for (i = 0; i < 5; i++)
        if (!tsearch(&keys[i], &root, icmp))
            return 1;
    int k6 = 6, k99 = 99, k3 = 3;
    if (!tfind(&k6, &root, icmp)) return 2;
    if (tfind(&k99, &root, icmp)) return 3;
    /* duplicate insert returns the existing node */
    if (**(int **)tsearch(&keys[0], &root, icmp) != 5) return 4;
    walk_buf[0] = 0;
    twalk(root, inorder_cb);
    if (strcmp(walk_buf, "13568")) return 5;
    if (!tdelete(&k3, &root, icmp)) return 6;
    if (tfind(&k3, &root, icmp)) return 7;
    freek_calls = 0;
    tdestroy(root, freek);
    if (freek_calls != 4) return 8; /* 5 inserted - 1 deleted */
    root = 0;

    /* lsearch/lfind */
    {
        int arr[4];
        size_t n = 0;
        int v10 = 10, v20 = 20, v99 = 99;
        if (lsearch(&v10, arr, &n, sizeof(int), icmp) != &arr[0] || n != 1)
            return 9;
        if (lsearch(&v20, arr, &n, sizeof(int), icmp) != &arr[1] || n != 2)
            return 10;
        if (lsearch(&v10, arr, &n, sizeof(int), icmp) != &arr[0] || n != 2)
            return 11; /* hit must not append */
        if (lfind(&v20, arr, &n, sizeof(int), icmp) != &arr[1]) return 12;
        if (lfind(&v99, arr, &n, sizeof(int), icmp)) return 13;
    }

    /* hsearch + hsearch_r (ENTRY by value — C-only surface, #671) */
    {
        static char ka[] = "alpha", kb[] = "beta", kg[] = "gamma";
        if (!hcreate(16)) return 14;
        ENTRY ea = {ka, &keys[0]}, eb = {kb, &keys[1]};
        if (!hsearch(ea, ENTER) || !hsearch(eb, ENTER)) return 15;
        ENTRY fa = {ka, 0}, fg = {kg, 0};
        ENTRY *hit = hsearch(fa, FIND);
        if (!hit || *(int *)hit->data != 5) return 16;
        if (hsearch(fg, FIND)) return 17;
        hdestroy();

        struct hsearch_data h1, h2;
        memset(&h1, 0, sizeof h1); memset(&h2, 0, sizeof h2);
        if (!hcreate_r(8, &h1) || !hcreate_r(8, &h2)) return 18;
        ENTRY e = {ka, &keys[2]}, *out = 0;
        if (!hsearch_r(e, ENTER, &out, &h1)) return 19;
        if (hsearch_r(fa, FIND, &out, &h2)) return 20; /* other table: miss */
        if (!hsearch_r(fa, FIND, &out, &h1) || *(int *)out->data != 8) return 21;
        hdestroy_r(&h1); hdestroy_r(&h2);
    }

    /* insque/remque */
    {
        struct qn { struct qn *next, *prev; int v; };
        static struct qn a, b, c;
        a.v = 1; b.v = 2; c.v = 3;
        insque(&a, 0); insque(&b, &a); insque(&c, &b);
        if (a.next != &b || b.next != &c || c.prev != &b) return 22;
        remque(&b);
        if (a.next != &c || c.prev != &a) return 23;
    }
    return 0;
}
