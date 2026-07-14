/* stddef.h — common definitions: size_t, ptrdiff_t, wchar_t, max_align_t
 * (pulled from <bits/alltypes.h> on demand) plus NULL and offsetof. */
#ifndef _STDDEF_H
#define _STDDEF_H

#define __NEED_size_t
#define __NEED_ptrdiff_t
#define __NEED_wchar_t
#define __NEED_max_align_t
#include <bits/alltypes.h>

#define offsetof(type, member) __builtin_offsetof(type, member)

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif /* _STDDEF_H */
