/* termios.h — POSIX terminal control (#675 stage D). Impl source/termios.c
 * over the whitelisted ioctl (<sys/ioctl.h>).
 *
 * The struct termios LAYOUT and every flag/speed value are the target's
 * NATIVE ones — the pointer passes raw to the kernel:
 *   - linux:  musl include/termios.h + arch/generic/bits/termios.h (same on
 *     x86_64 and aarch64; the kernel touches only the 36-byte prefix that
 *     matches asm-generic struct termios, NCCS 32 + speed fields are the
 *     glibc-compatible libc extension).
 *   - darwin: xnu sys/termios.h (tcflag_t/speed_t are unsigned long, NCCS 20,
 *     real c_ispeed/c_ospeed fields; sizeof == 72 is ABI — TIOCGETA encodes
 *     it). Values are literal-different from linux (e.g. TCIFLUSH 1 vs 0);
 *     source-portable code recompiles per target as everywhere else.
 * Feature-test gating (musl's _GNU_SOURCE/_BSD_SOURCE, xnu's _DARWIN_C_SOURCE)
 * is dropped: this libc exposes the full surface unconditionally. */
#ifndef _TERMIOS_H
#define _TERMIOS_H

#if defined(_WIN32)
#error "<termios.h> is not available on Windows (no MinGW/CRT counterpart; #677 audit)"
#endif

#include <c2go.h>

#define __NEED_pid_t
#define __NEED_struct_winsize

#include <bits/alltypes.h>

typedef unsigned char cc_t;

#if defined(__APPLE__)
/* ── darwin (xnu sys/termios.h) ─────────────────────────────────────── */

typedef unsigned long speed_t;
typedef unsigned long tcflag_t;

#define NCCS 20

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_cc[NCCS];
	speed_t  c_ispeed;
	speed_t  c_ospeed;
};

/* control characters (c_cc subscripts) */
#define VEOF      0
#define VEOL      1
#define VEOL2     2
#define VERASE    3
#define VWERASE   4
#define VKILL     5
#define VREPRINT  6
#define VINTR     8
#define VQUIT     9
#define VSUSP    10
#define VDSUSP   11
#define VSTART   12
#define VSTOP    13
#define VLNEXT   14
#define VDISCARD 15
#define VMIN     16
#define VTIME    17
#define VSTATUS  18

#define _POSIX_VDISABLE 0xff

/* c_iflag */
#define IGNBRK  0x00000001
#define BRKINT  0x00000002
#define IGNPAR  0x00000004
#define PARMRK  0x00000008
#define INPCK   0x00000010
#define ISTRIP  0x00000020
#define INLCR   0x00000040
#define IGNCR   0x00000080
#define ICRNL   0x00000100
#define IXON    0x00000200
#define IXOFF   0x00000400
#define IXANY   0x00000800
#define IMAXBEL 0x00002000
#define IUTF8   0x00004000

/* c_oflag */
#define OPOST  0x00000001
#define ONLCR  0x00000002
#define OXTABS 0x00000004
#define ONOEOT 0x00000008
#define OCRNL  0x00000010
#define ONOCR  0x00000020
#define ONLRET 0x00000040
#define OFILL  0x00000080
#define NLDLY  0x00000300
#define TABDLY 0x00000c04
#define CRDLY  0x00003000
#define FFDLY  0x00004000
#define BSDLY  0x00008000
#define VTDLY  0x00010000
#define OFDEL  0x00020000
#define NL0    0x00000000
#define NL1    0x00000100
#define NL2    0x00000200
#define NL3    0x00000300
#define TAB0   0x00000000
#define TAB1   0x00000400
#define TAB2   0x00000800
#define TAB3   0x00000004
#define CR0    0x00000000
#define CR1    0x00001000
#define CR2    0x00002000
#define CR3    0x00003000
#define FF0    0x00000000
#define FF1    0x00004000
#define BS0    0x00000000
#define BS1    0x00008000
#define VT0    0x00000000
#define VT1    0x00010000

/* c_cflag */
#define CIGNORE    0x00000001
#define CSIZE      0x00000300
#define CS5        0x00000000
#define CS6        0x00000100
#define CS7        0x00000200
#define CS8        0x00000300
#define CSTOPB     0x00000400
#define CREAD      0x00000800
#define PARENB     0x00001000
#define PARODD     0x00002000
#define HUPCL      0x00004000
#define CLOCAL     0x00008000
#define CCTS_OFLOW 0x00010000
#define CRTS_IFLOW 0x00020000
#define CRTSCTS    (CCTS_OFLOW | CRTS_IFLOW)
#define CDTR_IFLOW 0x00040000
#define CDSR_OFLOW 0x00080000
#define CCAR_OFLOW 0x00100000
#define MDMBUF     0x00100000

/* c_lflag */
#define ECHOKE     0x00000001
#define ECHOE      0x00000002
#define ECHOK      0x00000004
#define ECHO       0x00000008
#define ECHONL     0x00000010
#define ECHOPRT    0x00000020
#define ECHOCTL    0x00000040
#define ISIG       0x00000080
#define ICANON     0x00000100
#define ALTWERASE  0x00000200
#define IEXTEN     0x00000400
#define EXTPROC    0x00000800
#define TOSTOP     0x00400000
#define FLUSHO     0x00800000
#define NOKERNINFO 0x02000000
#define PENDIN     0x20000000
#define NOFLSH     0x80000000

/* tcsetattr() actions (TCSASOFT is the BSD CIGNORE carrier bit) */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCSASOFT  0x10

/* speeds are literal baud rates on darwin */
#define B0      0
#define B50     50
#define B75     75
#define B110    110
#define B134    134
#define B150    150
#define B200    200
#define B300    300
#define B600    600
#define B1200   1200
#define B1800   1800
#define B2400   2400
#define B4800   4800
#define B7200   7200
#define B9600   9600
#define B14400  14400
#define B19200  19200
#define B28800  28800
#define B38400  38400
#define B57600  57600
#define B76800  76800
#define B115200 115200
#define B230400 230400
#define EXTA    19200
#define EXTB    38400

/* tcflush() queue selectors / tcflow() actions (values differ from linux) */
#define TCIFLUSH  1
#define TCOFLUSH  2
#define TCIOFLUSH 3

#define TCOOFF 1
#define TCOON  2
#define TCIOFF 3
#define TCION  4

#else
/* ── linux (musl include/termios.h + arch/generic/bits/termios.h) ────── */

typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_line;
	cc_t     c_cc[NCCS];
	speed_t  __c_ispeed;
	speed_t  __c_ospeed;
};

/* control characters (c_cc subscripts) */
#define VINTR     0
#define VQUIT     1
#define VERASE    2
#define VKILL     3
#define VEOF      4
#define VTIME     5
#define VMIN      6
#define VSWTC     7
#define VSTART    8
#define VSTOP     9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

/* c_iflag */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* c_oflag */
#define OPOST  0000001
#define OLCUC  0000002
#define ONLCR  0000004
#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040
#define OFILL  0000100
#define OFDEL  0000200
#define NLDLY  0000400
#define NL0    0000000
#define NL1    0000400
#define CRDLY  0003000
#define CR0    0000000
#define CR1    0001000
#define CR2    0002000
#define CR3    0003000
#define TABDLY 0014000
#define TAB0   0000000
#define TAB1   0004000
#define TAB2   0010000
#define TAB3   0014000
#define BSDLY  0020000
#define BS0    0000000
#define BS1    0020000
#define FFDLY  0100000
#define FF0    0000000
#define FF1    0100000
#define VTDLY  0040000
#define VT0    0000000
#define VT1    0040000
#define XTABS  0014000

/* speeds are CBAUD-encoded codes on linux */
#define B0       0000000
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define B57600   0010001
#define B115200  0010002
#define B230400  0010003
#define B460800  0010004
#define B500000  0010005
#define B576000  0010006
#define B921600  0010007
#define B1000000 0010010
#define B1152000 0010011
#define B1500000 0010012
#define B2000000 0010013
#define B2500000 0010014
#define B3000000 0010015
#define B3500000 0010016
#define B4000000 0010017

#define EXTA    0000016
#define EXTB    0000017
#define CBAUD   0010017
#define CBAUDEX 0010000
#define CIBAUD  002003600000
#define CMSPAR  010000000000
#define CRTSCTS 020000000000

/* c_cflag */
#define CSIZE  0000060
#define CS5    0000000
#define CS6    0000020
#define CS7    0000040
#define CS8    0000060
#define CSTOPB 0000100
#define CREAD  0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL  0002000
#define CLOCAL 0004000

/* c_lflag */
#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define IEXTEN  0100000
#define EXTPROC 0200000

/* tcflow() actions / tcflush() queue selectors / tcsetattr() actions */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#endif /* __APPLE__ / linux */

speed_t cfgetospeed(const struct termios *)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfgetospeed", C2GO_GOABI0);
speed_t cfgetispeed(const struct termios *)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfgetispeed", C2GO_GOABI0);
int cfsetospeed(struct termios *, speed_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfsetospeed", C2GO_GOABI0);
int cfsetispeed(struct termios *, speed_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfsetispeed", C2GO_GOABI0);

int tcgetattr(int, struct termios *)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcgetattr", C2GO_GOABI0);
int tcsetattr(int, int, const struct termios *)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcsetattr", C2GO_GOABI0);

/* POSIX-2024 additions (musl has them; Apple libc does not — on darwin they
 * are native-kernel TIOCGWINSZ/TIOCSWINSZ one-liners, documented delta). */
int tcgetwinsize(int, struct winsize *)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcgetwinsize", C2GO_GOABI0);
int tcsetwinsize(int, const struct winsize *)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcsetwinsize", C2GO_GOABI0);

int tcsendbreak(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcsendbreak", C2GO_GOABI0);
int tcdrain(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcdrain", C2GO_GOABI0);
int tcflush(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcflush", C2GO_GOABI0);
int tcflow(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcflow", C2GO_GOABI0);

pid_t tcgetsid(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcgetsid", C2GO_GOABI0);

void cfmakeraw(struct termios *)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfmakeraw", C2GO_GOABI0);
int cfsetspeed(struct termios *, speed_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.cfsetspeed", C2GO_GOABI0);

#endif /* _TERMIOS_H */
