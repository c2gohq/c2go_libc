/* SPDX-License-Identifier: MIT
 *
 * Managed instantiation of musl's search tree, hash table, and queue
 * algorithms. All stored application and linkage pointers remain visible to
 * the Go GC; nodes and tables are allocated with typed gc_malloc. */

#define _GNU_SOURCE
#include <c2go/mlib/search.h>
#include <string.h>
#include <stdint.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

#define MLIB_SEARCH_MAXH (sizeof(void *) * 8 * 3 / 2)
#define MLIB_HSEARCH_MINSIZE 8
#define MLIB_HSEARCH_MAXSIZE ((size_t)-1 / 2 + 1)

struct mlib_tnode;
typedef struct mlib_tnode *managed mlib_tnode_ptr;

struct mlib_tnode {
    const void *managed key;
    mlib_tnode_ptr child[2];
    int height;
};

/* Keep deletion writes separate. LLVM may otherwise widen adjacent null
 * pointer stores into an integer store before c2go's write-barrier pass,
 * losing Go's deletion barrier for the old referents. */
static __attribute__((noinline)) void
mlib_tnode_clear_key(mlib_tnode_ptr node)
{
    node->key = 0;
}

static __attribute__((noinline)) void
mlib_tnode_clear_child(mlib_tnode_ptr node, unsigned int direction)
{
    node->child[direction] = 0;
}

static int mlib_tnode_height(mlib_tnode_ptr node)
{
    return node ? node->height : 0;
}

static mlib_tnode_ptr mlib_tnode_rotate(mlib_tnode_ptr node, int direction,
                                        int *height_delta)
{
    mlib_tnode_ptr child = node->child[direction];
    mlib_tnode_ptr middle = child->child[!direction];
    int old_height = node->height;
    int middle_height = mlib_tnode_height(middle);

    if (middle_height > mlib_tnode_height(child->child[direction])) {
        node->child[direction] = middle->child[!direction];
        child->child[!direction] = middle->child[direction];
        middle->child[!direction] = node;
        middle->child[direction] = child;
        node->height = middle_height;
        child->height = middle_height;
        middle->height = middle_height + 1;
    } else {
        node->child[direction] = middle;
        child->child[!direction] = node;
        node->height = middle_height + 1;
        child->height = middle_height + 2;
        middle = child;
    }
    *height_delta = middle->height - old_height;
    return middle;
}

static mlib_tnode_ptr mlib_tnode_balance(mlib_tnode_ptr node,
                                         int *height_delta)
{
    int left_height = mlib_tnode_height(node->child[0]);
    int right_height = mlib_tnode_height(node->child[1]);

    if (left_height - right_height + 1u < 3u) {
        int old_height = node->height;
        node->height = left_height < right_height ?
            right_height + 1 : left_height + 1;
        *height_delta = node->height - old_height;
        return node;
    }
    return mlib_tnode_rotate(node, left_height < right_height, height_delta);
}

c2go_extern void *managed mlib_tsearch(const void *managed key,
    void *managed *rootp,
    int (*compare)(const void *managed, const void *managed))
{
    mlib_tnode_ptr path[MLIB_SEARCH_MAXH];
    unsigned char direction[MLIB_SEARCH_MAXH];
    mlib_tnode_ptr *root = (mlib_tnode_ptr *)rootp;
    mlib_tnode_ptr node;
    mlib_tnode_ptr created;
    int depth = 0;

    if (!rootp) return 0;
    node = *root;
    while (node) {
        int order = compare(key, node->key);
        if (!order) return node;
        path[depth] = node;
        direction[depth] = order > 0;
        ++depth;
        node = node->child[direction[depth - 1]];
    }

    created = (mlib_tnode_ptr)gc_malloc(
        c2go_typeinfo(struct mlib_tnode), sizeof(*created));
    if (!created) return 0;
    created->key = key;
    created->child[0] = 0;
    created->child[1] = 0;
    created->height = 1;
    if (!depth)
        *root = created;
    else
        path[depth - 1]->child[direction[depth - 1]] = created;

    while (depth) {
        int height_delta;
        int index = --depth;
        mlib_tnode_ptr balanced =
            mlib_tnode_balance(path[index], &height_delta);

        if (!index)
            *root = balanced;
        else
            path[index - 1]->child[direction[index - 1]] = balanced;
        if (!height_delta) break;
    }
    return created;
}

c2go_extern void *managed mlib_tfind(const void *managed key,
    void *managed const *rootp,
    int (*compare)(const void *managed, const void *managed))
{
    mlib_tnode_ptr node;

    if (!rootp) return 0;
    node = *(mlib_tnode_ptr const *)rootp;
    while (node) {
        int order = compare(key, node->key);
        if (!order) break;
        node = node->child[order > 0];
    }
    return node;
}

c2go_extern void *managed mlib_tdelete(const void *managed key,
    void *managed *rootp,
    int (*compare)(const void *managed, const void *managed))
{
    mlib_tnode_ptr path[MLIB_SEARCH_MAXH + 1];
    unsigned char direction[MLIB_SEARCH_MAXH + 1];
    mlib_tnode_ptr *root = (mlib_tnode_ptr *)rootp;
    mlib_tnode_ptr node;
    mlib_tnode_ptr parent;
    mlib_tnode_ptr child;
    mlib_tnode_ptr removed;
    int depth = 0;

    if (!rootp) return 0;
    node = *root;
    while (node) {
        int order = compare(key, node->key);
        path[depth] = node;
        ++depth;
        if (!order) break;
        direction[depth - 1] = order > 0;
        node = node->child[direction[depth - 1]];
    }
    if (!node) return 0;

    parent = depth == 1 ? node : path[depth - 2];
    removed = node;
    if (node->child[0]) {
        mlib_tnode_ptr replaced = node;
        direction[depth - 1] = 0;
        node = node->child[0];
        while (node->child[1]) {
            path[depth] = node;
            direction[depth] = 1;
            ++depth;
            node = node->child[1];
        }
        path[depth++] = node;
        replaced->key = node->key;
        child = node->child[0];
        removed = node;
    } else {
        child = node->child[1];
    }

    if (depth == 1)
        *root = child;
    else
        path[depth - 2]->child[direction[depth - 2]] = child;

    mlib_tnode_clear_key(removed);
    mlib_tnode_clear_child(removed, 0);
    mlib_tnode_clear_child(removed, 1);

    while (--depth) {
        int height_delta;
        int index = depth - 1;
        mlib_tnode_ptr balanced =
            mlib_tnode_balance(path[index], &height_delta);

        if (!index)
            *root = balanced;
        else
            path[index - 1]->child[direction[index - 1]] = balanced;
        if (!height_delta) break;
    }
    return parent;
}

static void mlib_twalk_node(mlib_tnode_ptr node,
    void (*action)(const void *managed, VISIT, int), int depth)
{
    if (!node) return;
    if (node->height == 1) {
        action(node, leaf, depth);
        return;
    }
    action(node, preorder, depth);
    mlib_twalk_node(node->child[0], action, depth + 1);
    action(node, postorder, depth);
    mlib_twalk_node(node->child[1], action, depth + 1);
    action(node, endorder, depth);
}

c2go_extern void mlib_twalk(const void *managed root,
    void (*action)(const void *managed, VISIT, int))
{
    mlib_twalk_node((mlib_tnode_ptr)root, action, 0);
}

static void mlib_tdestroy_node(mlib_tnode_ptr node,
    void (*destroy_key)(void *managed))
{
    if (!node) return;
    mlib_tdestroy_node(node->child[0], destroy_key);
    mlib_tdestroy_node(node->child[1], destroy_key);
    if (destroy_key) destroy_key((void *managed)node->key);
    mlib_tnode_clear_key(node);
    mlib_tnode_clear_child(node, 0);
    mlib_tnode_clear_child(node, 1);
}

c2go_extern void mlib_tdestroy(void *managed root,
    void (*destroy_key)(void *managed))
{
    mlib_tdestroy_node((mlib_tnode_ptr)root, destroy_key);
}

struct mlib_hsearch_table {
    mlib_ENTRY *managed entries;
    size_t mask;
    size_t used;
};

static struct mlib_hsearch_data mlib_global_hsearch;

static __attribute__((noinline)) void
mlib_hsearch_clear_key(mlib_ENTRY *managed entry)
{
    entry->key = 0;
}

static __attribute__((noinline)) void
mlib_hsearch_clear_data(mlib_ENTRY *managed entry)
{
    entry->data = 0;
}

static size_t mlib_hsearch_hash(mlib_search_key_t key)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t hash = 0;
    while (*p) hash = 31 * hash + *p++;
    return hash;
}

static mlib_ENTRY *managed mlib_hsearch_lookup(mlib_search_key_t key,
    size_t hash, struct mlib_hsearch_data *table)
{
    size_t index, step;
    mlib_ENTRY *managed entry;

    for (index = hash, step = 1; ; index += step++) {
        entry = table->__tab->entries + (index & table->__tab->mask);
        if (!entry->key || strcmp((char *)entry->key, (char *)key) == 0)
            return entry;
    }
}

static int mlib_hsearch_resize(size_t count,
    struct mlib_hsearch_data *table)
{
    size_t new_size;
    size_t old_size = table->__tab->mask + 1;
    size_t index, step;
    mlib_ENTRY *managed old_entries = table->__tab->entries;
    mlib_ENTRY *managed new_entries;
    mlib_ENTRY *managed entry;
    mlib_ENTRY *managed target;

    if (count > MLIB_HSEARCH_MAXSIZE) count = MLIB_HSEARCH_MAXSIZE;
    for (new_size = MLIB_HSEARCH_MINSIZE; new_size < count; new_size *= 2) {}
    if (new_size > SIZE_MAX / sizeof(mlib_ENTRY)) return 0;
    new_entries = (mlib_ENTRY *managed)gc_malloc_array(
        c2go_typeinfo(mlib_ENTRY), sizeof(mlib_ENTRY), new_size);
    if (!new_entries) return 0;

    table->__tab->entries = new_entries;
    table->__tab->mask = new_size - 1;
    if (!old_entries) return 1;

    for (entry = old_entries; entry < old_entries + old_size; ++entry) {
        if (!entry->key) continue;
        for (index = mlib_hsearch_hash(entry->key), step = 1; ;
             index += step++) {
            target = new_entries + (index & table->__tab->mask);
            if (!target->key) break;
        }
        *target = *entry;
    }
    return 1;
}

c2go_extern int mlib_hcreate_r(size_t count,
    struct mlib_hsearch_data *table)
{
    struct mlib_hsearch_table *managed state;

    if (!table) return 0;
    state = (struct mlib_hsearch_table *managed)gc_malloc(
        c2go_typeinfo(struct mlib_hsearch_table), sizeof(*state));
    if (!state) return 0;
    table->__tab = state;
    if (!mlib_hsearch_resize(count, table)) {
        table->__tab = 0;
        return 0;
    }
    return 1;
}

c2go_extern void mlib_hdestroy_r(struct mlib_hsearch_data *table)
{
    if (!table || !table->__tab) return;
    table->__tab->entries = 0;
    table->__tab = 0;
}

c2go_extern int mlib_hsearch_r(mlib_ENTRY item, ACTION action,
    mlib_ENTRY *managed *result, struct mlib_hsearch_data *table)
{
    size_t hash;
    mlib_ENTRY *managed entry;

    if (!result || !table || !table->__tab || !table->__tab->entries ||
        !item.key) {
        if (result) *result = 0;
        return 0;
    }
    hash = mlib_hsearch_hash(item.key);
    entry = mlib_hsearch_lookup(item.key, hash, table);
    if (entry->key) {
        *result = entry;
        return 1;
    }
    if (action == FIND) {
        *result = 0;
        return 0;
    }
    *entry = item;
    if (++table->__tab->used >
        table->__tab->mask - table->__tab->mask / 4) {
        if (!mlib_hsearch_resize(2 * table->__tab->used, table)) {
            --table->__tab->used;
            mlib_hsearch_clear_key(entry);
            mlib_hsearch_clear_data(entry);
            *result = 0;
            return 0;
        }
        entry = mlib_hsearch_lookup(item.key, hash, table);
    }
    *result = entry;
    return 1;
}

c2go_extern int mlib_hcreate(size_t count)
{
    return mlib_hcreate_r(count, &mlib_global_hsearch);
}

c2go_extern void mlib_hdestroy(void)
{
    mlib_hdestroy_r(&mlib_global_hsearch);
}

c2go_extern mlib_ENTRY *managed mlib_hsearch(mlib_ENTRY item, ACTION action)
{
    mlib_ENTRY *managed result = 0;
    mlib_hsearch_r(item, action, &result, &mlib_global_hsearch);
    return result;
}

struct mlib_queue_node;
typedef struct mlib_queue_node *managed mlib_queue_node_ptr;
struct mlib_queue_node {
    mlib_queue_node_ptr next;
    mlib_queue_node_ptr previous;
};

static __attribute__((noinline)) void
mlib_queue_clear_link(mlib_queue_node_ptr node, unsigned int direction)
{
    if (direction)
        node->previous = 0;
    else
        node->next = 0;
}

c2go_extern void mlib_insque(void *managed element, void *managed predecessor)
{
    mlib_queue_node_ptr current = (mlib_queue_node_ptr)element;
    mlib_queue_node_ptr before = (mlib_queue_node_ptr)predecessor;

    if (!before) {
        mlib_queue_clear_link(current, 0);
        mlib_queue_clear_link(current, 1);
        return;
    }
    current->next = before->next;
    current->previous = before;
    before->next = current;
    if (current->next) current->next->previous = current;
}

c2go_extern void mlib_remque(void *managed element)
{
    mlib_queue_node_ptr current = (mlib_queue_node_ptr)element;

    if (current->next) current->next->previous = current->previous;
    if (current->previous) current->previous->next = current->next;
    mlib_queue_clear_link(current, 0);
    mlib_queue_clear_link(current, 1);
}

#pragma c2go pop
