/* termios_selftest.c — in-C exercise of the #675 stage D surface
 * (<termios.h> + the whitelisted <sys/ioctl.h>). Everything here runs
 * WITHOUT a tty: FIONREAD on a pipe is the positive whitelisted-ioctl path,
 * the tc* calls on pipe fds assert the kernel's ENOTTY truth, plus the
 * argument-validation EINVALs, the whitelist rejection, and the pure cf*
 * bit surface. ioctl is variadic (not Go-callable) and cf* take struct
 * termios* — C-only surface (search_selftest precedent). Positive tty
 * behavior is probe territory (host pty vs the native oracle).
 * Returns 0 on success, a distinct code per failing step. */
#if !defined(_WIN32)

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <c2go.h>

c2go_extern int termios_selftest(void)
{
    int fds[2];
    if (pipe(fds)) return 1;
    int rd = fds[0], wr = fds[1];

    /* FIONREAD through the full whitelist path: empty pipe, then 5 bytes. */
    int n = -1;
    if (ioctl(rd, FIONREAD, &n) != 0 || n != 0) return 2;
    if (write(wr, "hello", 5) != 5) return 3;
    if (ioctl(rd, FIONREAD, &n) != 0 || n != 5) return 4;

    /* ENOTTY truth on non-ttys. All kernel verdicts except tcgetpgrp's on
     * darwin: the raw TIOCGPGRP "succeeds" on a pipe there (xnu async-pgid),
     * and the ENOTTY comes from the Apple-mirrored library gate (termios.c). */
    struct termios t;
    errno = 0;
    if (tcgetattr(rd, &t) != -1 || errno != ENOTTY) return 5;
    struct winsize ws;
    errno = 0;
    if (tcgetwinsize(rd, &ws) != -1 || errno != ENOTTY) return 6;
    errno = 0;
    if (tcflush(rd, TCIOFLUSH) != -1 || errno != ENOTTY) return 7;
    errno = 0;
    if (tcgetpgrp(rd) != -1 || errno != ENOTTY) return 8;
    errno = 0;
    if (tcgetsid(rd) != -1 || errno != ENOTTY) return 9;

    /* argument validation (libc-side EINVAL in both references) */
    memset(&t, 0, sizeof t);
    errno = 0;
    if (tcsetattr(rd, 42, &t) != -1 || errno != EINVAL) return 10;

    /* whitelist rejection: not a supported command on either target */
    errno = 0;
    if (ioctl(rd, 0x5599, 0) != -1 || errno != ENOTTY) return 11;

    /* cf* pure-bit surface: set/get roundtrips (values are per-OS; the
     * roundtrip identity is the portable contract) */
    memset(&t, 0, sizeof t);
    if (cfsetospeed(&t, B9600) != 0 || cfgetospeed(&t) != B9600) return 12;
    if (cfsetispeed(&t, B19200) != 0 || cfgetispeed(&t) != B19200) return 13;
    memset(&t, 0, sizeof t);
    /* cfsetspeed: assert the output side only — the input side diverges by
     * design (musl records ispeed 0 = "follow output"; Apple sets both). */
    if (cfsetspeed(&t, B38400) != 0 || cfgetospeed(&t) != B38400) return 14;
#if !defined(__APPLE__)
    /* musl validates the CBAUD encoding; Apple accepts any literal rate */
    errno = 0;
    if (cfsetospeed(&t, 0x7fffffff) != -1 || errno != EINVAL) return 15;
#endif

    /* cfmakeraw invariants shared by both references */
    memset(&t, 0xff, sizeof t);
    cfmakeraw(&t);
    if (t.c_lflag & (ECHO|ICANON|ISIG)) return 16;
    if (t.c_oflag & OPOST) return 17;
    if ((t.c_cflag & CSIZE) != CS8) return 18;
    if (t.c_cc[VMIN] != 1 || t.c_cc[VTIME] != 0) return 19;

    if (close(rd) || close(wr)) return 20;
    return 0;
}

#endif /* !_WIN32 */
