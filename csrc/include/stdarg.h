/* stdarg.h — variable arguments (clang builtins). */
#ifndef _STDARG_H
#define _STDARG_H

#define __NEED_va_list
#include <bits/alltypes.h>   /* va_list */

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)

#endif /* _STDARG_H */
