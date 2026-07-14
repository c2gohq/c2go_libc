/* errno.h — error numbers.
 *
 * The `errno` lvalue is the per-goroutine C errno, reached through the Go-side
 * thread-local (ErrnoPtr) provided by <c2go.h>. The E* numbers are each
 * target's NATIVE values (musl on Linux, Darwin on macOS, MinGW on Windows) —
 * see <bits/errno.h> and ../PORTABILITY.md. The macro names are the portable
 * interface; the numbers differ per OS, so the Go syscall bridge maps host
 * errors to the names, never to fixed numbers. */
#ifndef _ERRNO_H
#define _ERRNO_H

#include <c2go.h>        /* the `errno` macro + __errno_location() */
#include <bits/errno.h>  /* E* number macros (per-OS native) */

#endif /* _ERRNO_H */
