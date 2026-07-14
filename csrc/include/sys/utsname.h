/* sys/utsname.h — system identification (#675 C wave 2b). Impl source/uname.c
 * over the __c2go_syscall_uname Go bridge (unix.Uname on both targets).
 *
 * struct utsname is the UNIFORM musl shape (six 65-byte fields) — the bridge
 * fills it field-by-field and truncates to 64 chars + NUL, so darwin's native
 * 256-byte fields lose nothing in practice (real values are short) and there
 * is no per-OS layout. musl gates the `domainname` member name behind
 * _GNU_SOURCE; exposed unconditionally here (this libc ships the full surface
 * — termios.h precedent). darwin has no domainname: filled empty. */
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#if defined(_WIN32)
#error "<sys/utsname.h> is not available on Windows (no MinGW/CRT counterpart; #677 audit)"
#endif

#include <c2go.h>

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

int uname(struct utsname *)
    c2go_linkname("github.com/c2gohq/c2go_libc.uname", C2GO_GOABI0);

#endif /* _SYS_UTSNAME_H */
