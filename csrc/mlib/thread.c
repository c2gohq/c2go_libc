/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed pthread lifecycle/key wrappers. The Go implementation and C
 * callback trampolines remain owned by root libc; this selectively-instantiated
 * layer changes only descriptor ownership from table ids/roots to direct
 * managed pointers. */

#include <c2go/mlib/pthread.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_create_managed", C2GO_GOABI0)
int __c2go_pthread_create_managed(mlib_pthread_t *,
    const mlib_pthread_attr_t *, mlib_pthread_start_routine_t, void *managed);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_join_managed", C2GO_GOABI0)
int __c2go_pthread_join_managed(mlib_pthread_t, void *managed *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadExit", C2GO_GOABI0)
void __c2go_pthread_exit_managed(void *managed);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_self_managed", C2GO_GOABI0)
mlib_pthread_t __c2go_pthread_self_managed(void);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_detach_managed", C2GO_GOABI0)
int __c2go_pthread_detach_managed(mlib_pthread_t);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_equal_managed", C2GO_GOABI0)
int __c2go_pthread_equal_managed(mlib_pthread_t, mlib_pthread_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadYield", C2GO_GOABI0)
int __c2go_pthread_yield_managed(void);

c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrInit", C2GO_GOABI0)
int __c2go_pthread_attr_init_managed(mlib_pthread_attr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrDestroy", C2GO_GOABI0)
int __c2go_pthread_attr_destroy_managed(mlib_pthread_attr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrSetDetachState", C2GO_GOABI0)
int __c2go_pthread_attr_setdetachstate_managed(mlib_pthread_attr_t *, int);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrSetStackSize", C2GO_GOABI0)
int __c2go_pthread_attr_setstacksize_managed(mlib_pthread_attr_t *, size_t);

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_pthread_key_create_managed", C2GO_GOABI0)
int __c2go_pthread_key_create_managed(mlib_pthread_key_t *,
    mlib_pthread_key_destructor_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadKeyDelete", C2GO_GOABI0)
int __c2go_pthread_key_delete_managed(mlib_pthread_key_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadGetSpecific", C2GO_GOABI0)
void *managed __c2go_pthread_getspecific_managed(mlib_pthread_key_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadSetSpecific", C2GO_GOABI0)
int __c2go_pthread_setspecific_managed(mlib_pthread_key_t,
    const void *managed);

c2go_extern int mlib_pthread_create(mlib_pthread_t *thread,
                                    const mlib_pthread_attr_t *attr,
                                    mlib_pthread_start_routine_t start,
                                    void *managed arg)
{
    return __c2go_pthread_create_managed(thread, attr, start, arg);
}

c2go_extern int mlib_pthread_join(mlib_pthread_t thread,
                                  void *managed *result)
{
    return __c2go_pthread_join_managed(thread, result);
}

c2go_extern void mlib_pthread_exit(void *managed result)
{
    __c2go_pthread_exit_managed(result);
}

c2go_extern mlib_pthread_t mlib_pthread_self(void)
{
    return __c2go_pthread_self_managed();
}

c2go_extern int mlib_pthread_detach(mlib_pthread_t thread)
{
    return __c2go_pthread_detach_managed(thread);
}

c2go_extern int mlib_pthread_equal(mlib_pthread_t left,
                                   mlib_pthread_t right)
{
    return __c2go_pthread_equal_managed(left, right);
}

c2go_extern int mlib_pthread_yield(void)
{
    return __c2go_pthread_yield_managed();
}

c2go_extern int mlib_pthread_attr_init(mlib_pthread_attr_t *attr)
{
    return __c2go_pthread_attr_init_managed(attr);
}

c2go_extern int mlib_pthread_attr_destroy(mlib_pthread_attr_t *attr)
{
    return __c2go_pthread_attr_destroy_managed(attr);
}

c2go_extern int mlib_pthread_attr_setdetachstate(mlib_pthread_attr_t *attr,
                                                 int state)
{
    return __c2go_pthread_attr_setdetachstate_managed(attr, state);
}

c2go_extern int mlib_pthread_attr_setstacksize(mlib_pthread_attr_t *attr,
                                               size_t size)
{
    return __c2go_pthread_attr_setstacksize_managed(attr, size);
}

c2go_extern int mlib_pthread_key_create(mlib_pthread_key_t *key,
                                        mlib_pthread_key_destructor_t destructor)
{
    return __c2go_pthread_key_create_managed(key, destructor);
}

c2go_extern int mlib_pthread_key_delete(mlib_pthread_key_t key)
{
    return __c2go_pthread_key_delete_managed(key);
}

c2go_extern void *managed mlib_pthread_getspecific(mlib_pthread_key_t key)
{
    return __c2go_pthread_getspecific_managed(key);
}

c2go_extern int mlib_pthread_setspecific(mlib_pthread_key_t key,
                                         const void *managed value)
{
    return __c2go_pthread_setspecific_managed(key, value);
}

#pragma c2go pop
