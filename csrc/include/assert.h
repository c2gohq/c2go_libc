/* assert.h — diagnostics. Deliberately re-includable: the assert macro tracks
 * NDEBUG at each inclusion. */

#undef assert
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__), 0)))
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
#define static_assert _Static_assert
#endif

#ifndef __C2GO_ASSERT_DECL
#define __C2GO_ASSERT_DECL
#include <c2go.h>   /* c2go_linkname / C2GO_GOABI0 (guarded; safe on re-include) */
_Noreturn void __assert_fail(const char *, const char *, int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.__assert_fail", C2GO_GOABI0);
#endif
