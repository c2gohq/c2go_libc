/* resource.c — getrusage/getrlimit (#675 C wave 2b). musl's versions are raw
 * syscalls plus time64/prlimit64 representation fixups; this libc reaches the
 * kernel through the Go syscall package, which already delivers the fixed-up
 * host representation (syscall.Getrusage / syscall.Getrlimit — the latter is
 * prlimit64-backed on linux), so the wrappers are pure __syscall_ret shapes
 * and the bridge fills the uniform musl-shaped structs field-by-field
 * (resource_unix.go). Value semantics stay kernel-native — see the
 * <sys/resource.h> header note. Empty on Windows (#677 audit). */
#if !defined(_WIN32)

#include <sys/resource.h>
#include <errno.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_getrusage", C2GO_GOABI0)
long __c2go_syscall_getrusage(int who, struct rusage *ru);
c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_getrlimit", C2GO_GOABI0)
long __c2go_syscall_getrlimit(int resource, struct rlimit *rlim);

c2go_extern int getrusage(int who, struct rusage *ru) {
	long r = __c2go_syscall_getrusage(who, ru);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

c2go_extern int getrlimit(int resource, struct rlimit *rlim) {
	long r = __c2go_syscall_getrlimit(resource, rlim);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

#endif /* !_WIN32 */
