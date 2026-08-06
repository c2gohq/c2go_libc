/* SPDX-License-Identifier: AGPL-3.0-only
 *
 * Managed ownership wrappers for the selectively instantiated musl/TRE
 * engine. The TRE implementation itself is compiled from the vendored source
 * with C2GO_MLIB_REGEX_BUILD; these wrappers install the per-call allocation
 * arena and expose the stable mlib_ namespace. */

#include <c2go/mlib/regex.h>
#include <stddef.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef void *managed mlib_regex_arena_pointer;

/* Keep managed publication and retirement in narrow noinline helpers. LLVM
 * otherwise combines adjacent null stores into a plain integer store before
 * c2go's barrier pass can preserve the old referents for concurrent GC. */
static __attribute__((noinline)) void
mlib_regex_store_root(mlib_regex_arena_pointer *slot,
                      mlib_regex_arena_pointer value)
{
    *slot = value;
}

static __attribute__((noinline)) void
mlib_regex_clear_root(mlib_regex_arena_pointer *slot)
{
    *slot = (void *)0;
}

mlib_regex_arena_pointer mlib_regex_arena_new(void)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.regexArenaNew", C2GO_GOABI0);
mlib_regex_arena_pointer
mlib_regex_arena_enter(mlib_regex_arena_pointer)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.regexArenaEnter", C2GO_GOABI0);
void mlib_regex_arena_leave(mlib_regex_arena_pointer)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.regexArenaLeave", C2GO_GOABI0);

int __mlib_regcomp_impl(mlib_regex_t *, const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.__mlib_regcomp_impl", C2GO_GOABI0);
int __mlib_regexec_impl(const mlib_regex_t *, const char *, size_t,
                        regmatch_t *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.__mlib_regexec_impl", C2GO_GOABI0);

c2go_extern int
mlib_regcomp(mlib_regex_t *restrict expression,
             const char *restrict pattern, int flags)
{
    mlib_regex_arena_pointer arena;
    mlib_regex_arena_pointer previous;
    int result;

    if (!expression || !pattern) return REG_BADPAT;
    arena = mlib_regex_arena_new();
    if (!arena) return REG_ESPACE;

    previous = mlib_regex_arena_enter(arena);
    expression->re_nsub = 0;
    mlib_regex_clear_root(&expression->__opaque);
    mlib_regex_store_root(&expression->__c2go_arena, arena);
    result = __mlib_regcomp_impl(expression, pattern, flags);
    mlib_regex_arena_leave(previous);

    if (result != REG_OK) {
        mlib_regex_clear_root(&expression->__opaque);
        mlib_regex_clear_root(&expression->__c2go_arena);
        expression->re_nsub = 0;
    }
    return result;
}

c2go_extern int
mlib_regexec(const mlib_regex_t *restrict expression,
             const char *restrict string, size_t match_count,
             regmatch_t *restrict matches, int flags)
{
    mlib_regex_arena_pointer temporary;
    mlib_regex_arena_pointer previous;
    int result;

    if (!expression || !expression->__opaque ||
        !expression->__c2go_arena || !string)
        return REG_BADPAT;

    /* Matching allocations never become part of the compiled expression.
     * Give each synchronous call its own goroutine-local arena, so concurrent
     * regexec calls share only the immutable TNFA and need no allocator lock. */
    temporary = mlib_regex_arena_new();
    if (!temporary) return REG_ESPACE;
    previous = mlib_regex_arena_enter(temporary);
    result = __mlib_regexec_impl(expression, string, match_count, matches,
                                 flags);
    mlib_regex_arena_leave(previous);
    return result;
}

c2go_extern void
mlib_regfree(mlib_regex_t *expression)
{
    if (!expression) return;

    /* Every persistent allocation is rooted by this arena. Dropping the two
     * carrier roots replaces TRE's allocation-by-allocation free traversal and
     * lets the Go GC reclaim the graph as a unit. */
    mlib_regex_clear_root(&expression->__opaque);
    mlib_regex_clear_root(&expression->__c2go_arena);
    expression->re_nsub = 0;
}

#pragma c2go pop
