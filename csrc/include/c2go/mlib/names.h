/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifdef C2GO_MLIB_NAMES_H

/* A later mlib header must not silently observe a different namespace mode
 * from the first one included in this translation unit. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifndef C2GO_MLIB_NAMES_UNPREFIXED
#error "C2GO_MLIB_UNPREFIXED must be set before the first mlib header"
#endif
#else
#ifdef C2GO_MLIB_NAMES_UNPREFIXED
#error "C2GO_MLIB_UNPREFIXED cannot change within a translation unit"
#endif
#endif

#else
#define C2GO_MLIB_NAMES_H

/*
 * Managed-libc namespace policy.
 *
 * The safe default keeps the managed surface visibly separate from the
 * ordinary, unmanaged libc surface:
 *
 *     mlib_sem_t s;
 *     mlib_sem_init(&s, 0, 1);
 *     mlib_pthread_mutex_t m;
 *     mlib_pthread_mutex_lock(&m);
 *
 * A translation unit that wants mlib to replace the corresponding C API may
 * define C2GO_MLIB_UNPREFIXED before its first mlib include:
 *
 *     #define C2GO_MLIB_UNPREFIXED 1
 *     #include <c2go/mlib/semaphore.h>
 *
 * That mode renames both state-bearing types and functions (sem_t/sem_init),
 * so an unmanaged sem_t can never be passed accidentally to a managed
 * implementation merely because only the function name changed.
 */
#define C2GO_MLIB_CAT_INNER(a, b) a##b
#define C2GO_MLIB_CAT(a, b) C2GO_MLIB_CAT_INNER(a, b)

#ifdef C2GO_MLIB_UNPREFIXED
#define C2GO_MLIB_NAMES_UNPREFIXED 1
#define C2GO_MLIB_NAME(name) name
#else
#define C2GO_MLIB_NAME(name) C2GO_MLIB_CAT(mlib_, name)
#endif

#endif /* first inclusion of C2GO_MLIB_NAMES_H */
