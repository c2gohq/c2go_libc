/* pthread.c — the C-side trampoline for pthread_create.
 *
 * A pthread's start routine is a c2go function pointer. The goroutine that backs
 * the thread is spawned in Go (thread.go PthreadCreate), but the fp itself must
 * be CALLED in C — exactly as source/qsort.c calls its comparator — because that
 * is how c2go lowers an indirect call through a c2go function pointer. This
 * trampoline is that call site: the new goroutine invokes it (through the
 * __c2go_pthread_run GoABI0 symbol), it runs the start routine on that
 * goroutine's stack, and it returns the void* result back to Go for
 * pthread_join to hand to the joiner. */
#include <c2go.h>
#include <pthread.h>
#include <errno.h>   /* EINVAL for mutexattr_settype (#664) */  /* attr typedefs for the #664 lifecycle functions */

c2go_extern void *__c2go_pthread_run(void *(*start)(void *), void *arg) {
    return start(arg);
}

/* Same idea for pthread_once's init routine, a void(*)(void). A dedicated
 * trampoline (rather than reusing __c2go_pthread_run) matters: that one returns
 * the callee's void* result, and reading a result a void function never wrote
 * would surface a garbage word into a GC-scanned slot — exactly the class of
 * "non-pointer in a pointer slot" hazard c2go's precise GC trips on. This one
 * returns void, so nothing garbage escapes. */
c2go_extern void __c2go_pthread_run_void(void (*fn)(void)) {
    fn();
}

/* And for a pthread_key_t destructor, a void(*)(void*). Same void-return
 * rationale as __c2go_pthread_run_void: nothing garbage escapes into a scanned
 * slot. arg is the thread-specific value being torn down. */
c2go_extern void __c2go_pthread_run_dtor(void (*fn)(void *), void *arg) {
    fn(arg);
}

/* ── attribute-object lifecycle (#664, musl thread/pthread_*attr*.c) ────────
 * The attr objects are plain structs; init resets to defaults, destroy is a
 * no-op (musl semantics). Completes the standard init->set->use->destroy
 * sequence (sqlite et al) that settype/setclock already assumed. */
c2go_extern int pthread_mutexattr_init(pthread_mutexattr_t *a)
{
	*a = (pthread_mutexattr_t){0};
	return 0;
}
c2go_extern int pthread_mutexattr_settype(pthread_mutexattr_t *a, int typ)
{
	/* #664: was a bare declaration riding the host-libc import stub — a
	 * hidden dependency this native body removes (musl semantics). Param is
	 * `typ`, not musl's `type`: c2go-bind does not sanitise Go keywords. */
	if (typ < 0 || typ > 2)
		return EINVAL;
	a->_type = typ;
	return 0;
}
c2go_extern int pthread_mutexattr_gettype(const pthread_mutexattr_t *a, int *typ)
{
	*typ = a->_type;
	return 0;
}
c2go_extern int pthread_mutexattr_destroy(pthread_mutexattr_t *a)
{
	(void)a;
	return 0;
}
c2go_extern int pthread_condattr_init(pthread_condattr_t *a)
{
	*a = (pthread_condattr_t){0};
	return 0;
}
c2go_extern int pthread_condattr_destroy(pthread_condattr_t *a)
{
	(void)a;
	return 0;
}
c2go_extern int pthread_rwlockattr_init(pthread_rwlockattr_t *a)
{
	*a = (pthread_rwlockattr_t){0};
	return 0;
}
c2go_extern int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a)
{
	(void)a;
	return 0;
}

/* pthread_atfork (#664): there is no fork under the Go runtime (design
 * decision — system()/popen() are the spawn surface), so the handlers can
 * never fire; registering them is therefore a complete, honest
 * implementation, not a stub. musl returns 0 unconditionally too. */
c2go_extern int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	(void)prepare; (void)parent; (void)child;
	return 0;
}
