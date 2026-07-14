/* sys/ioctl.h — device control (#675 stage D). Impl source/ioctl.c over the
 * __c2go_syscall_ioctl Go shim.
 *
 * WHITELIST: this libc supports exactly the commands defined below — the tty
 * surface consumed by <termios.h> plus FIONREAD. The C wrapper must know each
 * command's third-argument KIND (pointer vs int) to extract it from the c2go
 * vararg pack (typed va_arg, fcntl precedent in io_posix.c), so unknown
 * commands are REJECTED with errno = ENOTTY and never reach the kernel
 * (documented deviation: musl/Apple pass anything through; "inappropriate
 * ioctl" is the shape every prober already handles). Whitelisted commands
 * pass through raw — the structs they take are kernel-layout by construction
 * (per-OS <termios.h> / struct winsize).
 *
 * Values are the target's NATIVE ones: linux = musl arch/generic/bits/ioctl.h
 * (same on x86_64/aarch64); darwin = xnu sys/ttycom.h + sys/filio.h (size of
 * the encoded struct is ABI — termios.c pins it with _Static_assert). The
 * SIOC* networking block and the wider musl command set are deliberately
 * absent until implemented (no false advertising). */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#if defined(_WIN32)
#error "<sys/ioctl.h> is not available on Windows (no MinGW/CRT counterpart; #677 audit)"
#endif

#include <c2go.h>

#define __NEED_struct_winsize

#include <bits/alltypes.h>

#if defined(__APPLE__)
/* ── darwin (xnu sys/ioccom.h encoding) ─────────────────────────────── */

#define IOCPARM_MASK 0x1fff
#define IOC_VOID  0x20000000U
#define IOC_OUT   0x40000000U
#define IOC_IN    0x80000000U
#define IOC_INOUT (IOC_IN|IOC_OUT)

#define _IOC(inout,group,num,len) \
	((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define _IO(g,n)    _IOC(IOC_VOID, (g), (n), 0)
#define _IOR(g,n,t) _IOC(IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g,n,t) _IOC(IOC_IN, (g), (n), sizeof(t))
#define _IOWR(g,n,t) _IOC(IOC_INOUT, (g), (n), sizeof(t))

/* xnu sys/ttycom.h — literals so this header needs no struct termios;
 * termios.c _Static_asserts sizeof(struct termios)==72/winsize==8. */
#define TIOCGETA   0x40487413  /* _IOR('t', 19, struct termios) */
#define TIOCSETA   0x80487414  /* _IOW('t', 20, struct termios) */
#define TIOCSETAW  0x80487415  /* _IOW('t', 21, struct termios) */
#define TIOCSETAF  0x80487416  /* _IOW('t', 22, struct termios) */
#define TIOCFLUSH  0x80047410  /* _IOW('t', 16, int) */
#define TIOCDRAIN  0x2000745e  /* _IO('t', 94) */
#define TIOCSTOP   0x2000746f  /* _IO('t', 111) */
#define TIOCSTART  0x2000746e  /* _IO('t', 110) */
#define TIOCSBRK   0x2000747b  /* _IO('t', 123) */
#define TIOCCBRK   0x2000747a  /* _IO('t', 122) */
#define TIOCGPGRP  0x40047477  /* _IOR('t', 119, int) */
#define TIOCSPGRP  0x80047476  /* _IOW('t', 118, int) */
#define TIOCGSID   0x40047463  /* _IOR('t', 99, int) — sys/ioctl_compat.h,
                                * handled by the tty core (Apple tcgetsid) */
#define TIOCGWINSZ 0x40087468  /* _IOR('t', 104, struct winsize) */
#define TIOCSWINSZ 0x80087467  /* _IOW('t', 103, struct winsize) */
#define FIONREAD   0x4004667f  /* _IOR('f', 127, int) — sys/filio.h */

#else
/* ── linux (musl arch/generic/bits/ioctl.h) ─────────────────────────── */

#define _IOC(a,b,c,d) ( ((a)<<30) | ((b)<<8) | (c) | ((d)<<16) )
#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U

#define _IO(a,b)     _IOC(_IOC_NONE,(a),(b),0)
#define _IOW(a,b,c)  _IOC(_IOC_WRITE,(a),(b),sizeof(c))
#define _IOR(a,b,c)  _IOC(_IOC_READ,(a),(b),sizeof(c))
#define _IOWR(a,b,c) _IOC(_IOC_READ|_IOC_WRITE,(a),(b),sizeof(c))

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TCSBRK     0x5409
#define TCXONC     0x540A
#define TCFLSH     0x540B
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD   0x541B
#define TIOCINQ    FIONREAD
#define TIOCGSID   0x5429

#endif /* __APPLE__ / linux */

int ioctl(int, int, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.ioctl", C2GO_GOABI0);

#endif /* _SYS_IOCTL_H */
