/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed scandir. The pointer array and directory-entry copies are Go-heap
 * objects, so growth and sorting must preserve precise GC metadata and execute
 * pointer stores through c2go's write barriers. */

#include <c2go/mlib/dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef struct dirent *managed mlib_dirent_pointer;

/* A one-pointer record gives gc_malloc_array an element type whose bitmap can
 * be repeated across the allocation. Its layout is intentionally identical to
 * struct dirent *, which is the public scandir result element type. */
typedef struct {
    mlib_dirent_pointer value;
} mlib_dirent_slot;

typedef mlib_dirent_pointer *managed mlib_dirent_vector;

static int mlib_scandir_less(mlib_dirent_slot *items, size_t a, size_t b,
                             int (*cmp)(const struct dirent **,
                                        const struct dirent **))
{
    return cmp((const struct dirent **)&items[a].value,
               (const struct dirent **)&items[b].value) < 0;
}

static void mlib_scandir_swap(mlib_dirent_slot *a, mlib_dirent_slot *b)
{
    mlib_dirent_pointer value = a->value;
    a->value = b->value;
    b->value = value;
}

static void mlib_scandir_sift_down(mlib_dirent_slot *items, size_t root,
                                   size_t end,
                                   int (*cmp)(const struct dirent **,
                                              const struct dirent **))
{
    for (;;) {
        size_t child;

        if (end == 0 || root > (end - 1) / 2) return;
        child = root * 2 + 1;
        if (child < end && mlib_scandir_less(items, child, child + 1, cmp))
            ++child;
        if (!mlib_scandir_less(items, root, child, cmp)) return;
        mlib_scandir_swap(&items[root], &items[child]);
        root = child;
    }
}

/* In-place heapsort uses typed pointer assignments for every swap. Ordinary
 * qsort performs byte copies and therefore cannot safely mutate this managed
 * pointer array. */
static __attribute__((noinline)) void
mlib_scandir_sort(mlib_dirent_slot *items, size_t count,
                  int (*cmp)(const struct dirent **,
                             const struct dirent **))
{
    size_t start, end;

    if (count < 2) return;
    start = (count - 2) / 2 + 1;
    while (start != 0) {
        --start;
        mlib_scandir_sift_down(items, start, count - 1, cmp);
    }
    end = count - 1;
    while (end != 0) {
        mlib_scandir_swap(&items[0], &items[end]);
        --end;
        mlib_scandir_sift_down(items, 0, end, cmp);
    }
}

/* Preserve an AS1 value at the public POSIX-compatible triple-pointer
 * boundary. The destination may be a caller's heap field, so it needs a write
 * barrier even though the API spelling remains struct dirent ***. */
static __attribute__((noinline)) void
mlib_scandir_store_result(struct dirent ***result, mlib_dirent_slot *items)
{
    *(mlib_dirent_vector *)result = (mlib_dirent_vector)items;
}

c2go_extern int mlib_scandir(const char *path, struct dirent ***result,
                             int (*select_entry)(const struct dirent *),
                             int (*compare_entry)(const struct dirent **,
                                                  const struct dirent **))
{
    mlib_DIR *dir;
    mlib_dirent_pointer entry;
    mlib_dirent_slot *items = (void *)0;
    size_t count = 0, capacity = 0;
    int old_errno = errno;

    mlib_scandir_store_result(result, (void *)0);
    dir = mlib_opendir(path);
    if (!dir) return -1;

    while ((errno = 0),
           (entry = (mlib_dirent_pointer)mlib_readdir(dir))) {
        mlib_dirent_slot *grown;
        mlib_dirent_pointer copy;
        size_t next, i;

        if (select_entry && !select_entry((const struct dirent *)entry))
            continue;
        if (count == INT_MAX) {
            errno = ENOMEM;
            break;
        }
        if (count == capacity) {
            if (capacity > (SIZE_MAX - 1) / 2) {
                errno = ENOMEM;
                break;
            }
            next = capacity * 2 + 1;
            if (next > SIZE_MAX / sizeof(*grown)) {
                errno = ENOMEM;
                break;
            }
            grown = gc_malloc_array(c2go_typeinfo(mlib_dirent_slot),
                                    sizeof(*grown), next);
            if (!grown) {
                errno = ENOMEM;
                break;
            }
            for (i = 0; i < count; ++i) grown[i].value = items[i].value;
            items = grown;
            capacity = next;
        }

        /* struct dirent has no pointers, so a noscan managed blob is exact. */
        copy = (mlib_dirent_pointer)gc_malloc((void *)0, sizeof(*copy));
        if (!copy) {
            errno = ENOMEM;
            break;
        }
        *copy = *entry;
        items[count++].value = copy;
    }

    (void)mlib_closedir(dir);
    entry = (mlib_dirent_pointer)0;
    dir = (void *)0;
    if (errno) return -1;
    errno = old_errno;

    if (compare_entry) mlib_scandir_sort(items, count, compare_entry);
    mlib_scandir_store_result(result, items);
    return (int)count;
}

#pragma c2go pop
