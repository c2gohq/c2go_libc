/* ioctl.c — the whitelisted ioctl entry (#675 stage D).
 *
 * ioctl is variadic, and the c2go vararg pack is TYPED: the wrapper must
 * extract the third argument with the kind the caller passed (pointer vs
 * int) — reading an int cell as void* is not defined under the pack model.
 * That is exactly why the command set is a whitelist (fcntl precedent,
 * io_posix.c): each supported command's argument kind is known here, and an
 * unknown command is refused with ENOTTY before it can reach the kernel
 * (see <sys/ioctl.h> for the policy note). Whitelisted commands pass
 * through raw: fd/req/arg go straight to the SYS_ioctl shim (ioctl_unix.go),
 * and the structs involved are kernel-layout by construction (<termios.h>).
 * The whole file is empty on Windows (gen.sh compiles every source for
 * every target; there is no ioctl surface there — #677 audit). */
#if !defined(_WIN32)

#include <sys/ioctl.h>
#include <errno.h>
#include <stdarg.h>
#include <c2go.h>

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_ioctl", C2GO_GOABI0)
long __c2go_syscall_ioctl(int fd, int req, void *arg);

c2go_extern int ioctl(int fd, int req, ...)
{
	void *arg;
	va_list ap;
	va_start(ap, req);
	switch ((unsigned)req) {
#if defined(__APPLE__)
	/* no-argument commands (_IO 'void'): nothing to extract — the
	 * conventional caller-supplied 0 (Apple libc style) stays unread. */
	case TIOCDRAIN:
	case TIOCSTOP:
	case TIOCSTART:
	case TIOCSBRK:
	case TIOCCBRK:
		arg = 0;
		break;
	/* pointer-argument commands */
	case TIOCGETA:
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
	case TIOCFLUSH:
	case TIOCGPGRP:
	case TIOCSPGRP:
	case TIOCGSID:
	case TIOCGWINSZ:
	case TIOCSWINSZ:
	case FIONREAD:
		arg = va_arg(ap, void *);
		break;
#else
	/* int-argument commands: the VALUE travels in the arg word itself */
	case TCSBRK:
	case TCXONC:
	case TCFLSH:
		arg = (void *)(long)va_arg(ap, int);
		break;
	/* pointer-argument commands */
	case TCGETS:
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
	case TIOCGPGRP:
	case TIOCSPGRP:
	case TIOCGSID:
	case TIOCGWINSZ:
	case TIOCSWINSZ:
	case FIONREAD:
		arg = va_arg(ap, void *);
		break;
#endif
	default:
		va_end(ap);
		errno = ENOTTY;
		return -1;
	}
	va_end(ap);
	long r = __c2go_syscall_ioctl(fd, req, arg);
	if (r < 0) { errno = (int)-r; return -1; }
	return (int)r;
}

#endif /* !_WIN32 */
