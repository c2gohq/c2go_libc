/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_ENTRY
#error "C2GO_MLIB_TEST_ENTRY must name the managed hash entry type"
#endif
#ifndef C2GO_MLIB_TEST_HSEARCH_DATA
#error "C2GO_MLIB_TEST_HSEARCH_DATA must name the managed hash carrier type"
#endif
#ifndef C2GO_MLIB_TEST_HCREATE
#error "C2GO_MLIB_TEST_HCREATE must name the managed global hash constructor"
#endif
#ifndef C2GO_MLIB_TEST_HDESTROY
#error "C2GO_MLIB_TEST_HDESTROY must name the managed global hash destructor"
#endif
#ifndef C2GO_MLIB_TEST_HSEARCH
#error "C2GO_MLIB_TEST_HSEARCH must name the managed global hash lookup"
#endif
#ifndef C2GO_MLIB_TEST_HCREATE_R
#error "C2GO_MLIB_TEST_HCREATE_R must name the managed hash constructor"
#endif
#ifndef C2GO_MLIB_TEST_HDESTROY_R
#error "C2GO_MLIB_TEST_HDESTROY_R must name the managed hash destructor"
#endif
#ifndef C2GO_MLIB_TEST_HSEARCH_R
#error "C2GO_MLIB_TEST_HSEARCH_R must name the managed hash lookup"
#endif
#ifndef C2GO_MLIB_TEST_TSEARCH
#error "C2GO_MLIB_TEST_TSEARCH must name the managed tree insertion"
#endif
#ifndef C2GO_MLIB_TEST_TFIND
#error "C2GO_MLIB_TEST_TFIND must name the managed tree lookup"
#endif
#ifndef C2GO_MLIB_TEST_TDELETE
#error "C2GO_MLIB_TEST_TDELETE must name the managed tree deletion"
#endif
#ifndef C2GO_MLIB_TEST_TWALK
#error "C2GO_MLIB_TEST_TWALK must name the managed tree walk"
#endif
#ifndef C2GO_MLIB_TEST_TDESTROY
#error "C2GO_MLIB_TEST_TDESTROY must name the managed tree destruction"
#endif
#ifndef C2GO_MLIB_TEST_INSQUE
#error "C2GO_MLIB_TEST_INSQUE must name the managed queue insertion"
#endif
#ifndef C2GO_MLIB_TEST_REMQUE
#error "C2GO_MLIB_TEST_REMQUE must name the managed queue removal"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the exported selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

#include <string.h>

typedef int *managed c2go_mlib_search_int_pointer;

struct c2go_mlib_search_payload {
    int value;
    c2go_mlib_search_int_pointer nested;
};
typedef struct c2go_mlib_search_payload *managed
    c2go_mlib_search_payload_pointer;

struct c2go_mlib_search_item {
    int key;
    c2go_mlib_search_payload_pointer payload;
};
typedef struct c2go_mlib_search_item *managed c2go_mlib_search_item_pointer;
typedef c2go_mlib_search_item_pointer *managed c2go_mlib_search_item_slot;

struct c2go_mlib_search_queue {
    struct c2go_mlib_search_queue *managed next;
    struct c2go_mlib_search_queue *managed previous;
    c2go_mlib_search_payload_pointer payload;
    int key;
};
typedef struct c2go_mlib_search_queue *managed
    c2go_mlib_search_queue_pointer;

static int c2go_mlib_search_walk_count;
static int c2go_mlib_search_walk_error;
static int c2go_mlib_search_destroy_count;

static char *managed c2go_mlib_search_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *managed copy =
        (char *managed)gc_malloc((void *)0, length);

    if (copy) memcpy((char *)copy, text, length);
    return copy;
}

static c2go_mlib_search_payload_pointer
c2go_mlib_search_payload(int value)
{
    c2go_mlib_search_payload_pointer payload;
    c2go_mlib_search_int_pointer nested;

    payload = (c2go_mlib_search_payload_pointer)gc_malloc(
        c2go_typeinfo(struct c2go_mlib_search_payload), sizeof(*payload));
    if (!payload) return 0;
    nested = (c2go_mlib_search_int_pointer)gc_malloc(
        (void *)0, sizeof(*nested));
    if (!nested) return 0;
    *nested = value + 1000;
    payload->value = value;
    payload->nested = nested;
    return payload;
}

static c2go_mlib_search_item_pointer c2go_mlib_search_item(int key)
{
    c2go_mlib_search_item_pointer item;

    item = (c2go_mlib_search_item_pointer)gc_malloc(
        c2go_typeinfo(struct c2go_mlib_search_item), sizeof(*item));
    if (!item) return 0;
    item->key = key;
    item->payload = c2go_mlib_search_payload(key * 10 + 7);
    if (!item->payload) return 0;
    return item;
}

static int c2go_mlib_search_compare(const void *managed left,
                                    const void *managed right)
{
    const struct c2go_mlib_search_item *managed a =
        (const struct c2go_mlib_search_item *managed)left;
    const struct c2go_mlib_search_item *managed b =
        (const struct c2go_mlib_search_item *managed)right;

    return (a->key > b->key) - (a->key < b->key);
}

static int c2go_mlib_search_check_item(
    c2go_mlib_search_item_pointer item, int key)
{
    int value = key * 10 + 7;

    return item && item->key == key && item->payload &&
        item->payload->value == value && item->payload->nested &&
        *item->payload->nested == value + 1000;
}

static void c2go_mlib_search_walk(const void *managed node,
                                  VISIT visit, int depth)
{
    c2go_mlib_search_item_pointer item;

    (void)depth;
    if (visit != preorder && visit != leaf) return;
    item = *(c2go_mlib_search_item_slot)node;
    if (!c2go_mlib_search_check_item(item, item ? item->key : -1))
        c2go_mlib_search_walk_error = 1;
    ++c2go_mlib_search_walk_count;
    C2GO_MLIB_TEST_FORCE_GC();
}

static void c2go_mlib_search_destroy(void *managed value)
{
    c2go_mlib_search_item_pointer item =
        (c2go_mlib_search_item_pointer)value;

    if (!c2go_mlib_search_check_item(item, item ? item->key : -1))
        c2go_mlib_search_walk_error = 1;
    ++c2go_mlib_search_destroy_count;
    C2GO_MLIB_TEST_FORCE_GC();
}

static int c2go_mlib_search_tree_test(void)
{
    static const int inserted[] = {4, 2, 6, 1, 3, 5, 7, 9, 8};
    static const int removed[] = {4, 1, 8};
    void *managed root = 0;
    c2go_mlib_search_item_pointer item = 0;
    c2go_mlib_search_item_pointer found;
    void *managed node;
    size_t i;

    for (i = 0; i < sizeof(inserted) / sizeof(inserted[0]); ++i) {
        item = c2go_mlib_search_item(inserted[i]);
        if (!item) return 1;
        node = C2GO_MLIB_TEST_TSEARCH(item, &root,
                                      c2go_mlib_search_compare);
        if (!node || *(c2go_mlib_search_item_slot)node != item) return 2;
        item = 0;
    }
    C2GO_MLIB_TEST_FORCE_GC();

    for (i = 0; i < sizeof(inserted) / sizeof(inserted[0]); ++i) {
        item = c2go_mlib_search_item(inserted[i]);
        if (!item) return 3;
        node = C2GO_MLIB_TEST_TFIND(item, &root,
                                    c2go_mlib_search_compare);
        if (!node) return 4;
        found = *(c2go_mlib_search_item_slot)node;
        if (!c2go_mlib_search_check_item(found, inserted[i])) return 5;
        item = 0;
    }

    for (i = 0; i < sizeof(removed) / sizeof(removed[0]); ++i) {
        item = c2go_mlib_search_item(removed[i]);
        if (!item) return 6;
        if (!C2GO_MLIB_TEST_TDELETE(item, &root,
                                    c2go_mlib_search_compare)) return 7;
        if (C2GO_MLIB_TEST_TFIND(item, &root,
                                 c2go_mlib_search_compare)) return 8;
        item = 0;
        C2GO_MLIB_TEST_FORCE_GC();
    }

    c2go_mlib_search_walk_count = 0;
    c2go_mlib_search_walk_error = 0;
    C2GO_MLIB_TEST_TWALK(root, c2go_mlib_search_walk);
    if (c2go_mlib_search_walk_error || c2go_mlib_search_walk_count != 6)
        return 9;

    c2go_mlib_search_destroy_count = 0;
    C2GO_MLIB_TEST_TDESTROY(root, c2go_mlib_search_destroy);
    if (c2go_mlib_search_walk_error || c2go_mlib_search_destroy_count != 6)
        return 10;
    root = 0;
    node = 0;
    found = 0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

static int c2go_mlib_search_hash_check(C2GO_MLIB_TEST_ENTRY *managed entry,
                                       const char *key, int value)
{
    c2go_mlib_search_payload_pointer payload;

    if (!entry || strcmp((char *)entry->key, key)) return 0;
    payload = (c2go_mlib_search_payload_pointer)entry->data;
    return payload && payload->value == value && payload->nested &&
        *payload->nested == value + 1000;
}

static int c2go_mlib_search_hash_test(void)
{
    static const char *const keys[] = {
        "zero", "one", "two", "three", "four", "five",
        "six", "seven", "eight", "nine", "ten", "eleven"
    };
    C2GO_MLIB_TEST_HSEARCH_DATA table = {0};
    C2GO_MLIB_TEST_ENTRY item = {0};
    C2GO_MLIB_TEST_ENTRY *managed result = 0;
    c2go_mlib_search_payload_pointer payload = 0;
    char *managed key = 0;
    size_t i;

    if (!C2GO_MLIB_TEST_HCREATE_R(2, &table)) return 1;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        key = c2go_mlib_search_string(keys[i]);
        payload = c2go_mlib_search_payload((int)i + 200);
        if (!key || !payload) return 2;
        item.key = key;
        item.data = payload;
        if (!C2GO_MLIB_TEST_HSEARCH_R(item, ENTER, &result, &table) ||
            result->key != key || result->data != payload)
            return 3;
        item.key = 0;
        item.data = 0;
        result = 0;
        key = 0;
        payload = 0;
    }
    C2GO_MLIB_TEST_FORCE_GC();

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        key = c2go_mlib_search_string(keys[i]);
        if (!key) return 4;
        item.key = key;
        item.data = 0;
        if (!C2GO_MLIB_TEST_HSEARCH_R(item, FIND, &result, &table) ||
            !c2go_mlib_search_hash_check(result, keys[i], (int)i + 200))
            return 5;
        item.key = 0;
        result = 0;
        key = 0;
        C2GO_MLIB_TEST_FORCE_GC();
    }
    C2GO_MLIB_TEST_HDESTROY_R(&table);
    if (table.__tab) return 6;

    if (!C2GO_MLIB_TEST_HCREATE(2)) return 7;
    key = c2go_mlib_search_string("global");
    payload = c2go_mlib_search_payload(313);
    if (!key || !payload) return 8;
    item.key = key;
    item.data = payload;
    result = C2GO_MLIB_TEST_HSEARCH(item, ENTER);
    if (!c2go_mlib_search_hash_check(result, "global", 313)) return 9;
    item.key = 0;
    item.data = 0;
    result = 0;
    key = 0;
    payload = 0;
    C2GO_MLIB_TEST_FORCE_GC();

    key = c2go_mlib_search_string("global");
    if (!key) return 10;
    item.key = key;
    result = C2GO_MLIB_TEST_HSEARCH(item, FIND);
    if (!c2go_mlib_search_hash_check(result, "global", 313)) return 11;
    C2GO_MLIB_TEST_HDESTROY();
    item.key = 0;
    result = 0;
    key = 0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

static c2go_mlib_search_queue_pointer c2go_mlib_search_queue(int key)
{
    c2go_mlib_search_queue_pointer node;

    node = (c2go_mlib_search_queue_pointer)gc_malloc(
        c2go_typeinfo(struct c2go_mlib_search_queue), sizeof(*node));
    if (!node) return 0;
    node->key = key;
    node->payload = c2go_mlib_search_payload(key * 10);
    if (!node->payload) return 0;
    return node;
}

static int c2go_mlib_search_queue_test(void)
{
    c2go_mlib_search_queue_pointer first = c2go_mlib_search_queue(1);
    c2go_mlib_search_queue_pointer second = c2go_mlib_search_queue(2);
    c2go_mlib_search_queue_pointer third = c2go_mlib_search_queue(3);

    if (!first || !second || !third) return 1;
    C2GO_MLIB_TEST_INSQUE(first, 0);
    C2GO_MLIB_TEST_INSQUE(second, first);
    C2GO_MLIB_TEST_INSQUE(third, second);
    second = 0;
    third = 0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!first->next || first->next->key != 2 ||
        !first->next->payload || first->next->payload->value != 20 ||
        !first->next->next || first->next->next->key != 3 ||
        first->next->next->previous != first->next)
        return 2;

    second = first->next;
    C2GO_MLIB_TEST_REMQUE(second);
    if (second->next || second->previous || !first->next ||
        first->next->key != 3 || first->next->previous != first)
        return 3;
    second = 0;
    C2GO_MLIB_TEST_FORCE_GC();

    third = first->next;
    C2GO_MLIB_TEST_REMQUE(third);
    if (first->next || third->next || third->previous) return 4;
    third = 0;
    first = 0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

static int c2go_mlib_search_scalar_compare(const void *left,
                                           const void *right)
{
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

static int c2go_mlib_search_scalar_lsearch_test(void)
{
    int values[4] = {1, 3, 5, 0};
    int needle = 4;
    size_t count = 3;
    int *found = lsearch(&needle, values, &count, sizeof(values[0]),
                         c2go_mlib_search_scalar_compare);

    return !found || *found != 4 || count != 4;
}

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    int rc;

    rc = c2go_mlib_search_tree_test();
    if (rc) return 100 + rc;
    rc = c2go_mlib_search_hash_test();
    if (rc) return 200 + rc;
    rc = c2go_mlib_search_queue_test();
    if (rc) return 300 + rc;
    rc = c2go_mlib_search_scalar_lsearch_test();
    if (rc) return 400 + rc;
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_REMQUE
#undef C2GO_MLIB_TEST_INSQUE
#undef C2GO_MLIB_TEST_TDESTROY
#undef C2GO_MLIB_TEST_TWALK
#undef C2GO_MLIB_TEST_TDELETE
#undef C2GO_MLIB_TEST_TFIND
#undef C2GO_MLIB_TEST_TSEARCH
#undef C2GO_MLIB_TEST_HSEARCH_R
#undef C2GO_MLIB_TEST_HDESTROY_R
#undef C2GO_MLIB_TEST_HCREATE_R
#undef C2GO_MLIB_TEST_HSEARCH
#undef C2GO_MLIB_TEST_HDESTROY
#undef C2GO_MLIB_TEST_HCREATE
#undef C2GO_MLIB_TEST_HSEARCH_DATA
#undef C2GO_MLIB_TEST_ENTRY
