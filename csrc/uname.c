/* uname.c — system identification (#675 C wave 2b). musl's uname is a raw
 * SYS_uname one-liner; this libc cannot issue raw syscalls, so the shape is
 * the musl one-liner over the Go bridge (uname_unix.go: unix.Uname on both
 * targets, per-OS field widths marshalled into the uniform musl-shaped
 * struct — see <sys/utsname.h>). Empty on Windows (#677 audit). */
#if !defined(_WIN32)

#include <sys/utsname.h>
#include <errno.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_uname", C2GO_GOABI0)
long __c2go_syscall_uname(struct utsname *uts);

c2go_extern int uname(struct utsname *uts) {
	long r = __c2go_syscall_uname(uts);
	if (r < 0) { errno = (int)-r; return -1; }
	return 0;
}

#endif /* !_WIN32 */
