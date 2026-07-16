/* conf.c — cross-platform conf surface (#675, user decision 1+3): these four
 * are backed by platform-independent Go primitives (os.Hostname, crypto/rand,
 * os.Getpagesize, runtime.NumCPU), so they are provided on EVERY target —
 * including windows, where the reference-existence line is relaxed to
 * "the Go primitive honestly delivers the semantics" (gethostname is in
 * MinGW anyway, via winsock). pipe2/kill/sync etc. stay unix-only in
 * unistd2.c. Wrappers follow the shim contract (Go returns -errno). */
#include <unistd.h>
#include <errno.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_gethostname", C2GO_GOABI0)
long __c2go_gethostname(char *buf, size_t n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_getentropy", C2GO_GOABI0)
long __c2go_getentropy(void *buf, size_t n);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_getpagesize", C2GO_GOABI0)
long __c2go_getpagesize(void);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_sysconf", C2GO_GOABI0)
long __c2go_sysconf(int name);

#define SYSCALL_RET(r) do { if ((r) < 0) { errno = (int)-(r); return -1; } } while (0)

c2go_extern int gethostname(char *buf, size_t n) {
	long r = __c2go_gethostname(buf, n);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int getentropy(void *buf, size_t n) {
	long r = __c2go_getentropy(buf, n);
	SYSCALL_RET(r);
	return 0;
}

c2go_extern int getpagesize(void) {
	return (int)__c2go_getpagesize();
}

c2go_extern long sysconf(int name) {
	long r = __c2go_sysconf(name);
	SYSCALL_RET(r);
	return r;
}
