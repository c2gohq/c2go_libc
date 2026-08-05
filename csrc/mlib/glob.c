/* SPDX-License-Identifier: MIT
 *
 * Derived from musl src/regex/glob.c. This managed instantiation replaces the
 * linked-list nodes, strings, result vector, DIR carrier, growth, sorting, and
 * release policy while retaining musl's pathname-matching algorithm. */

#define _BSD_SOURCE
#include <c2go/mlib/glob.h>
#include <c2go/mlib/dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* The pragma makes pointer fields GC-scanned, but character pointers also
 * need the explicit managed pointer type so their LLVM values stay in AS1.
 * That provenance is what makes pointer moves through the match list and the
 * result vector receive Go write barriers. */
typedef char *managed mlib_glob_string;

struct mlib_match {
    struct mlib_match *next;
    mlib_glob_string name;
};

typedef struct {
    mlib_glob_string value;
} mlib_glob_slot;

static int mlib_match_append(struct mlib_match **tail, const char *name,
                             size_t length, int mark)
{
    struct mlib_match *node;
    mlib_glob_string copy;

    if (length > SIZE_MAX - 2) return -1;
    copy = (mlib_glob_string)gc_malloc((void *)0, length + 2);
    if (!copy) return -1;
    node = gc_malloc(c2go_typeinfo(struct mlib_match), sizeof(*node));
    if (!node) return -1;

    memcpy((char *)copy, name, length + 1);
    if (mark && length && name[length - 1] != '/') {
        copy[length] = '/';
        copy[length + 1] = 0;
    }
    node->name = copy;
    node->next = (void *)0;
    (*tail)->next = node;
    *tail = node;
    return 0;
}

static int mlib_do_glob(char *buffer, size_t position, int type, char *pattern,
                        int flags,
                        int (*error_function)(const char *path, int error),
                        struct mlib_match **tail)
{
    ptrdiff_t i = 0, j = 0;
    int in_bracket = 0, overflow = 0;
    char *separator;
    char saved_separator = '/';
    mlib_DIR *directory;
    struct dirent *entry;
    int old_errno, read_error;

    if (!type && !(flags & GLOB_MARK)) type = DT_REG;
    if (*pattern && type != DT_DIR) type = 0;
    while (position + 1 < PATH_MAX && *pattern == '/')
        buffer[position++] = *pattern++;

    for (; pattern[i] != '*' && pattern[i] != '?' &&
           (!in_bracket || pattern[i] != ']'); ++i) {
        if (!pattern[i]) {
            if (overflow) return 0;
            pattern += i;
            position += j;
            i = j = 0;
            break;
        } else if (pattern[i] == '[') {
            in_bracket = 1;
        } else if (pattern[i] == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (in_bracket && pattern[i + 1] == ']') break;
            if (!pattern[i + 1]) return 0;
            ++i;
        }
        if (pattern[i] == '/') {
            if (overflow) return 0;
            in_bracket = 0;
            pattern += i + 1;
            i = -1;
            position += j + 1;
            j = -1;
        }
        if (position + (j + 1) < PATH_MAX) {
            buffer[position + j++] = pattern[i];
        } else if (in_bracket) {
            overflow = 1;
        } else {
            return 0;
        }
        type = 0;
    }
    buffer[position] = 0;
    if (!*pattern) {
        struct stat status;

        if ((flags & GLOB_MARK) && (!type || type == DT_LNK) &&
            !stat(buffer, &status)) {
            type = S_ISDIR(status.st_mode) ? DT_DIR : DT_REG;
        }
        if (!type && lstat(buffer, &status)) {
            if (errno != ENOENT &&
                (error_function(buffer, errno) || (flags & GLOB_ERR)))
                return GLOB_ABORTED;
            return 0;
        }
        if (mlib_match_append(tail, buffer, position,
                              (flags & GLOB_MARK) && type == DT_DIR))
            return GLOB_NOSPACE;
        return 0;
    }

    separator = strchr(pattern, '/');
    if (separator && !(flags & GLOB_NOESCAPE)) {
        char *p;
        for (p = separator; p > pattern && p[-1] == '\\'; --p) {}
        if ((separator - p) % 2) {
            --separator;
            saved_separator = '\\';
        }
    }

    directory = mlib_opendir(position ? buffer : ".");
    if (!directory) {
        if (error_function(buffer, errno) || (flags & GLOB_ERR))
            return GLOB_ABORTED;
        return 0;
    }
    old_errno = errno;
    while ((errno = 0), (entry = mlib_readdir(directory))) {
        size_t length;
        int fnmatch_flags, result;

        if (separator && entry->d_type && entry->d_type != DT_DIR &&
            entry->d_type != DT_LNK)
            continue;

        length = strlen(entry->d_name);
        if (length >= PATH_MAX - position) continue;
        if (separator) *separator = 0;

        fnmatch_flags = ((flags & GLOB_NOESCAPE) ? FNM_NOESCAPE : 0) |
                        ((!(flags & GLOB_PERIOD)) ? FNM_PERIOD : 0);
        if (fnmatch(pattern, entry->d_name, fnmatch_flags)) continue;

        if (separator && (flags & GLOB_PERIOD) && entry->d_name[0] == '.' &&
            (!entry->d_name[1] ||
             (entry->d_name[1] == '.' && !entry->d_name[2])) &&
            fnmatch(pattern, entry->d_name, fnmatch_flags | FNM_PERIOD))
            continue;

        memcpy(buffer + position, entry->d_name, length + 1);
        if (separator) *separator = saved_separator;
        result = mlib_do_glob(buffer, position + length, entry->d_type,
                              separator ? separator : "", flags,
                              error_function, tail);
        if (result) {
            mlib_closedir(directory);
            return result;
        }
    }
    read_error = errno;
    if (separator) *separator = saved_separator;
    mlib_closedir(directory);
    if (read_error &&
        (error_function(buffer, errno) || (flags & GLOB_ERR)))
        return GLOB_ABORTED;
    errno = old_errno;
    return 0;
}

static int mlib_ignore_glob_error(const char *path, int error)
{
    (void)path;
    (void)error;
    return 0;
}

static int mlib_glob_slot_less(mlib_glob_slot *items, size_t a, size_t b)
{
    return strcmp((char *)items[a].value, (char *)items[b].value) < 0;
}

static void mlib_glob_slot_swap(mlib_glob_slot *a, mlib_glob_slot *b)
{
    mlib_glob_string value = a->value;
    a->value = b->value;
    b->value = value;
}

static void mlib_glob_sift_down(mlib_glob_slot *items, size_t root, size_t end)
{
    for (;;) {
        size_t child;

        if (end == 0 || root > (end - 1) / 2) return;
        child = root * 2 + 1;
        if (child < end && mlib_glob_slot_less(items, child, child + 1))
            ++child;
        if (!mlib_glob_slot_less(items, root, child)) return;
        mlib_glob_slot_swap(&items[root], &items[child]);
        root = child;
    }
}

static void mlib_glob_sort(mlib_glob_slot *items, size_t count)
{
    size_t start, end;

    if (count < 2) return;
    start = (count - 2) / 2 + 1;
    while (start) {
        --start;
        mlib_glob_sift_down(items, start, count - 1);
    }
    end = count - 1;
    while (end) {
        mlib_glob_slot_swap(&items[0], &items[end]);
        --end;
        mlib_glob_sift_down(items, 0, end);
    }
}

static int mlib_glob_result_size(size_t offset, size_t old_count,
                                 size_t new_count, size_t *base,
                                 size_t *total)
{
    if (offset > SIZE_MAX - old_count) return -1;
    *base = offset + old_count;
    if (*base == SIZE_MAX || new_count > SIZE_MAX - *base - 1) return -1;
    *total = *base + new_count + 1;
    if (*total > SIZE_MAX / sizeof(mlib_glob_slot)) return -1;
    return 0;
}

/* Keep the carrier-root update out of aggregate-store folding. Otherwise an
 * optimizing frontend can merge gl_pathc/gl_pathv clearing into memset, which
 * erases the managed pointer value before the write-barrier pass sees it. */
static __attribute__((noinline)) void
mlib_glob_store_paths(mlib_glob_t *result, mlib_glob_slot *items)
{
    result->gl_pathv = (char **managed)items;
}

c2go_extern int mlib_glob(const char *restrict pattern, int flags,
                          int (*error_function)(const char *path, int error),
                          mlib_glob_t *restrict result)
{
    struct mlib_match head = { .next = (void *)0, .name = (void *)0 };
    struct mlib_match *tail = &head, *match;
    mlib_glob_slot *old_items, *items;
    size_t count, i, offset, old_count, base, total;
    int error = 0;
    char buffer[PATH_MAX];

    if (!error_function) error_function = mlib_ignore_glob_error;
    offset = (flags & GLOB_DOOFFS) ? result->gl_offs : 0;
    old_count = (flags & GLOB_APPEND) ? result->gl_pathc : 0;
    old_items = (flags & GLOB_APPEND) ?
        (mlib_glob_slot *)result->gl_pathv : (void *)0;

    if (!(flags & GLOB_APPEND)) {
        result->gl_offs = offset;
        result->gl_pathc = 0;
        mlib_glob_store_paths(result, (void *)0);
    }

    if (*pattern) {
        size_t length = strlen(pattern);
        char *copy;
        char *current;
        size_t position = 0;

        if (length == SIZE_MAX) return GLOB_NOSPACE;
        copy = gc_malloc((void *)0, length + 1);
        if (!copy) return GLOB_NOSPACE;
        memcpy(copy, pattern, length + 1);
        buffer[0] = 0;
        current = copy;
        if ((flags & (GLOB_TILDE | GLOB_TILDE_CHECK)) && *copy == '~') {
            char *home;
            char delimiter, *name_end = strchrnul(current + 1, '/');
            size_t home_length = 0;

            delimiter = *name_end;
            if (delimiter) *name_end++ = 0;
            current = name_end;
            home = copy[1] ? (void *)0 : getenv("HOME");
            if (!home) {
                error = GLOB_NOMATCH;
            } else {
                while (home_length < PATH_MAX - 2 && home[home_length])
                    ++home_length;
                if (home[home_length]) {
                    error = GLOB_NOMATCH;
                } else {
                    memcpy(buffer, home, home_length);
                    if ((buffer[home_length] = delimiter))
                        buffer[++home_length] = 0;
                    position = home_length;
                }
            }
        }
        if (!error)
            error = mlib_do_glob(buffer, position, 0, current, flags,
                                 error_function, &tail);
    }

    if (error == GLOB_NOSPACE) return error;

    count = 0;
    for (match = head.next; match; match = match->next) {
        if (count == SIZE_MAX) return GLOB_NOSPACE;
        ++count;
    }
    if (!count) {
        if (flags & GLOB_NOCHECK) {
            if (mlib_match_append(&tail, pattern, strlen(pattern), 0))
                return GLOB_NOSPACE;
            count = 1;
        } else if (!error) {
            return GLOB_NOMATCH;
        }
    }

    if (mlib_glob_result_size(offset, old_count, count, &base, &total))
        return GLOB_NOSPACE;
    items = gc_malloc_array(c2go_typeinfo(mlib_glob_slot),
                            sizeof(*items), total);
    if (!items) return GLOB_NOSPACE;

    if ((flags & GLOB_APPEND) && old_items) {
        for (i = 0; i < base; ++i) items[i].value = old_items[i].value;
    }
    for (i = 0, match = head.next; i < count;
         ++i, match = match->next)
        items[base + i].value = match->name;
    items[base + count].value = (void *)0;

    mlib_glob_store_paths(result, items);
    result->gl_pathc = old_count + count;
    if (!(flags & GLOB_NOSORT)) mlib_glob_sort(items + base, count);
    return error;
}

c2go_extern void mlib_globfree(mlib_glob_t *result)
{
    mlib_glob_store_paths(result, (void *)0);
    result->gl_pathc = 0;
}

#pragma c2go pop
