/* termios.c — POSIX terminal control (#675 stage D): thin wrappers over the
 * whitelisted ioctl (source/ioctl.c).
 *
 *   - linux branch: musl src/termios/ verbatim. Deltas: tcdrain is a plain
 *     call, not a cancellation point (this libc has no thread cancellation);
 *     tcgetwinsize/tcsetwinsize go through ioctl() instead of raw syscall().
 *   - darwin branch: Apple Libc (FreeBSD-heritage) gen/termios.c shapes over
 *     the xnu command set (TIOCGETA family — darwin has no TCGETS). Deltas:
 *     tcsendbreak's 0.4s break hold is usleep(400000) instead of the
 *     reference's select-as-sleep; tcgetwinsize/tcsetwinsize have no Apple
 *     counterpart (POSIX-2024) and are the native-command one-liners.
 *
 * tcgetpgrp/tcsetpgrp (declared in <unistd.h>) live here too, PER-OS:
 * musl's plain one-liners on linux, but Apple adds a POSIX-conformance
 * gate on darwin — the raw commands "succeed" on a pipe there (xnu
 * pipe_ioctl async-pgid, host-verified), see the darwin branch note.
 * The whole file is empty on Windows (no termios there — #677 audit). */
#if !defined(_WIN32)

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <c2go.h>

/* Layout pins: darwin's TIOCGETA/TIOCGWINSZ encode sizeof in the command
 * word; linux's TCGETS copies the 36-byte kernel prefix of the musl shape.
 * The <sys/ioctl.h> command literals rely on these sizes. */
_Static_assert(sizeof(struct winsize) == 8, "winsize = 4 x u16 on both targets");
#if defined(__APPLE__)
_Static_assert(sizeof(struct termios) == 72, "TIOCGETA encodes length 72 (0x48)");
#else
_Static_assert(sizeof(struct termios) == 60, "musl/glibc-compatible linux shape");
#endif

#if defined(__APPLE__)
/* ── darwin: Apple Libc termios.c ───────────────────────────────────── */

/* kernel FREAD/FWRITE (xnu sys/fcntl.h, kernel scope): the TIOCFLUSH
 * argument encoding used by Apple's tcflush. */
#define __FREAD  0x0001
#define __FWRITE 0x0002

c2go_extern int tcgetattr(int fd, struct termios *t)
{
	return ioctl(fd, TIOCGETA, t);
}

c2go_extern int tcsetattr(int fd, int opt, const struct termios *t)
{
	struct termios localterm;

	if (opt & TCSASOFT) {
		localterm = *t;
		localterm.c_cflag |= CIGNORE;
		t = &localterm;
	}
	switch (opt & ~TCSASOFT) {
	case TCSANOW:
		return ioctl(fd, TIOCSETA, t);
	case TCSADRAIN:
		return ioctl(fd, TIOCSETAW, t);
	case TCSAFLUSH:
		return ioctl(fd, TIOCSETAF, t);
	default:
		errno = EINVAL;
		return -1;
	}
}

c2go_extern speed_t cfgetospeed(const struct termios *t)
{
	return t->c_ospeed;
}

c2go_extern speed_t cfgetispeed(const struct termios *t)
{
	return t->c_ispeed;
}

c2go_extern int cfsetospeed(struct termios *t, speed_t speed)
{
	t->c_ospeed = speed;
	return 0;
}

c2go_extern int cfsetispeed(struct termios *t, speed_t speed)
{
	t->c_ispeed = speed;
	return 0;
}

c2go_extern int cfsetspeed(struct termios *t, speed_t speed)
{
	t->c_ispeed = t->c_ospeed = speed;
	return 0;
}

c2go_extern void cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IXOFF|INPCK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IGNPAR);
	t->c_iflag |= IGNBRK;
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|IEXTEN|NOFLSH|TOSTOP|PENDIN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8|CREAD;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}

c2go_extern int tcsendbreak(int fd, int len)
{
	/* nonzero duration is implementation-defined; Apple ignores it and
	 * holds the break bit for 0.4s */
	(void)len;
	if (ioctl(fd, TIOCSBRK, 0) == -1)
		return -1;
	usleep(400000);
	if (ioctl(fd, TIOCCBRK, 0) == -1)
		return -1;
	return 0;
}

c2go_extern int tcdrain(int fd)
{
	return ioctl(fd, TIOCDRAIN, 0);
}

c2go_extern int tcflush(int fd, int which)
{
	int com;

	switch (which) {
	case TCIFLUSH:
		com = __FREAD;
		break;
	case TCOFLUSH:
		com = __FWRITE;
		break;
	case TCIOFLUSH:
		com = __FREAD | __FWRITE;
		break;
	default:
		errno = EINVAL;
		return -1;
	}
	return ioctl(fd, TIOCFLUSH, &com);
}

c2go_extern int tcflow(int fd, int action)
{
	struct termios term;
	unsigned char c;

	switch (action) {
	case TCOOFF:
		return ioctl(fd, TIOCSTOP, 0);
	case TCOON:
		return ioctl(fd, TIOCSTART, 0);
	case TCION:
	case TCIOFF:
		if (tcgetattr(fd, &term) == -1)
			return -1;
		c = term.c_cc[action == TCIOFF ? VSTOP : VSTART];
		if (c != _POSIX_VDISABLE && write(fd, &c, sizeof(c)) == -1)
			return -1;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

/* POSIX-conformance gate (Apple Libc behavior, host-verified): the RAW
 * TIOCGPGRP/TIOCSPGRP commands "succeed" on a darwin PIPE — xnu's
 * pipe_ioctl services them as the pipe's async-I/O pgid — so the kernel
 * alone cannot deliver the ENOTTY that POSIX requires of tc[gs]etpgrp on
 * non-terminals. Apple gates in the library; mirror it via isatty. The
 * raw ioctl() path is left kernel-honest (a direct TIOCGPGRP on a pipe
 * behaves exactly like the native one). */
c2go_extern pid_t tcgetpgrp(int fd)
{
	int pgrp;
	if (!isatty(fd)) {
		errno = ENOTTY;
		return -1;
	}
	if (ioctl(fd, TIOCGPGRP, &pgrp) < 0)
		return -1;
	return pgrp;
}

c2go_extern int tcsetpgrp(int fd, pid_t pgrp)
{
	int pgrp_int = pgrp;
	if (!isatty(fd)) {
		errno = ENOTTY;
		return -1;
	}
	return ioctl(fd, TIOCSPGRP, &pgrp_int);
}

#else
/* ── linux: musl src/termios/ ───────────────────────────────────────── */

c2go_extern int tcgetattr(int fd, struct termios *tio)
{
	if (ioctl(fd, TCGETS, tio))
		return -1;
	return 0;
}

c2go_extern int tcsetattr(int fd, int act, const struct termios *tio)
{
	if (act < 0 || act > 2) {
		errno = EINVAL;
		return -1;
	}
	return ioctl(fd, TCSETS+act, tio);
}

c2go_extern speed_t cfgetospeed(const struct termios *tio)
{
	return tio->c_cflag & CBAUD;
}

c2go_extern speed_t cfgetispeed(const struct termios *tio)
{
	return (tio->c_cflag & CIBAUD) / (CIBAUD/CBAUD);
}

c2go_extern int cfsetospeed(struct termios *tio, speed_t speed)
{
	if (speed & ~CBAUD) {
		errno = EINVAL;
		return -1;
	}
	tio->c_cflag &= ~CBAUD;
	tio->c_cflag |= speed;
	return 0;
}

c2go_extern int cfsetispeed(struct termios *tio, speed_t speed)
{
	if (speed & ~CBAUD) {
		errno = EINVAL;
		return -1;
	}
	tio->c_cflag &= ~CIBAUD;
	tio->c_cflag |= speed * (CIBAUD/CBAUD);
	return 0;
}

c2go_extern int cfsetspeed(struct termios *tio, speed_t speed)
{
	int r = cfsetospeed(tio, speed);
	if (!r) cfsetispeed(tio, 0);
	return r;
}

c2go_extern void cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}

c2go_extern int tcsendbreak(int fd, int dur)
{
	/* nonzero duration is implementation-defined, so ignore it */
	return ioctl(fd, TCSBRK, 0);
}

c2go_extern int tcdrain(int fd)
{
	return ioctl(fd, TCSBRK, 1);
}

c2go_extern int tcflush(int fd, int queue)
{
	return ioctl(fd, TCFLSH, queue);
}

c2go_extern int tcflow(int fd, int action)
{
	return ioctl(fd, TCXONC, action);
}

/* musl src/unistd/ shapes — no library gate needed on linux: the kernel
 * itself ENOTTYs tty commands on non-terminals (TIOCGPGRP is tty-only). */
c2go_extern pid_t tcgetpgrp(int fd)
{
	int pgrp;
	if (ioctl(fd, TIOCGPGRP, &pgrp) < 0)
		return -1;
	return pgrp;
}

c2go_extern int tcsetpgrp(int fd, pid_t pgrp)
{
	int pgrp_int = pgrp;
	return ioctl(fd, TIOCSPGRP, &pgrp_int);
}

#endif /* __APPLE__ / linux */

/* ── OS-uniform bodies (command values resolve per-OS) ──────────────── */

c2go_extern pid_t tcgetsid(int fd)
{
	int sid;
	if (ioctl(fd, TIOCGSID, &sid) < 0)
		return -1;
	return sid;
}

c2go_extern int tcgetwinsize(int fd, struct winsize *wsz)
{
	return ioctl(fd, TIOCGWINSZ, wsz);
}

c2go_extern int tcsetwinsize(int fd, const struct winsize *wsz)
{
	return ioctl(fd, TIOCSWINSZ, wsz);
}

#endif /* !_WIN32 */
