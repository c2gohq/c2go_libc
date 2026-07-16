/* iconv.c — the (iconv_t)-1 sentinel boundary (#648 follow-up to the iconv
 * pointer-handle fix).
 *
 * The Go bridge (../iconv.go) deals ONLY in real pointers: iconv_open returns
 * the *iconvState (a Go heap object, rooted by the handle table while C holds
 * it) or nil on failure. POSIX's failure sentinel is (iconv_t)-1 — a
 * manufactured NON-pointer value that must never reach a Go pointer slot (a
 * prior integer-handle design parked such values in unsafe.Pointer stack
 * slots and the precise stack scan threw "invalid pointer found on stack").
 * These wrappers keep the sentinel strictly in the C world: nil -> -1 on the
 * way out, -1 screened to EBADF on the way in. The C world tolerates -1 in
 * its own iconv_t variables (unmanaged pointers are conservatively scanned,
 * never precisely trusted).
 *
 * KEEPCASE keeps the exported Go names lowercase (iconv_open, not IconvOpen)
 * so they cannot collide with the hand-written Go bridge functions. */
#include <c2go.h>
#include <iconv.h>
#include <errno.h>

c2go_linkname("github.com/c2gohq/c2go_libc.IconvOpen", C2GO_GOABI0)
void  *__c2go_iconv_open(const char *to, const char *from);
/* The object-like `c2go_extern` macro shadows the attribute spelling (see
 * c2go.h): lift it locally so the KEEPCASE argument form is expressible. */
#pragma push_macro("c2go_extern")
#undef c2go_extern
c2go_linkname("github.com/c2gohq/c2go_libc.Iconv", C2GO_GOABI0)
size_t __c2go_iconv(void *cd, char **in, size_t *inleft, char **out, size_t *outleft);
c2go_linkname("github.com/c2gohq/c2go_libc.IconvClose", C2GO_GOABI0)
int    __c2go_iconv_close(void *cd);

__attribute__((c2go_extern(C2GO_KEEPCASE)))
iconv_t iconv_open(const char *to, const char *from) {
	void *cd = __c2go_iconv_open(to, from);
	return cd ? (iconv_t)cd : (iconv_t)-1;   /* errno already set by the bridge */
}

__attribute__((c2go_extern(C2GO_KEEPCASE)))
size_t iconv(iconv_t cd, char **restrict in, size_t *restrict inleft,
             char **restrict out, size_t *restrict outleft) {
	if (cd == (iconv_t)-1 || !cd) { errno = EBADF; return (size_t)-1; }
	return __c2go_iconv(cd, in, inleft, out, outleft);
}

__attribute__((c2go_extern(C2GO_KEEPCASE)))
int iconv_close(iconv_t cd) {
	if (cd == (iconv_t)-1 || !cd) { errno = EBADF; return -1; }
	return __c2go_iconv_close(cd);
}

#pragma pop_macro("c2go_extern")
