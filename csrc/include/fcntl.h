/* fcntl.h — file control (open flags in <bits/fcntl.h>, per-OS native). */
#ifndef _FCNTL_H
#define _FCNTL_H

#define __NEED_mode_t
#define __NEED_off_t
#include <bits/alltypes.h>
#include <c2go.h>

#include <bits/fcntl.h>   /* O_* flag bits (native per-OS) */

/* fcntl() commands — c2go-libc's own command codes (its fcntl impl maps them
 * to the Go bridge); F_DUPFD..F_SETFL match Linux/macOS 0..4. */
#define F_DUPFD    0
#define F_GETFD    1
#define F_SETFD    2
#define F_GETFL    3
#define F_SETFL    4
#define FD_CLOEXEC 1

/* Record locks (#648, Unix only — MinGW-w64 has no fcntl). The command codes
 * and l_type values are c2go-UNIFORM (the native ones DIFFER per OS: linux
 * F_SETLK=6 vs darwin=8, darwin F_WRLCK=3 vs linux=1); the C fcntl wrapper
 * routes lock commands to a dedicated Go bridge that translates both the
 * command and the flock layout to the host's (../flock.go over
 * syscall.FcntlFlock), so the numbers here never reach a raw syscall. The
 * 0x10xx range cannot collide with any pass-through native command. */
#if !defined(_WIN32)
#define F_GETLK  0x1001
#define F_SETLK  0x1002
#define F_SETLKW 0x1003

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

/* struct flock — a UNIFORM c2go layout (the cStat pattern): the Go bridge
 * copies field-by-field into the host's syscall.Flock_t (whose member ORDER
 * differs between linux and darwin), never a raw-layout pass-through. */
struct flock {
	short l_type;    /* F_RDLCK / F_WRLCK / F_UNLCK (uniform values above) */
	short l_whence;  /* SEEK_SET / SEEK_CUR / SEEK_END */
	long long l_start;
	long long l_len;   /* 0 = to EOF */
	int   l_pid;       /* F_GETLK: the blocking lock's owner */
};
#endif /* !_WIN32 */

/* open/fcntl: a C wrapper on both OSes (source/io_posix.c over a Go syscall shim
 * on Unix, source/io_windows.c over the CRT on Windows), reached through the same
 * lowercase C symbol. */
int open(const char *, int, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.open", C2GO_GOABI0);
/* openat + the AT_* constants (#675; the shim is syscall.Openat). The AT_*
 * values are the HOST kernel's — they go straight into the syscall. */
#if !defined(_WIN32)
#if defined(__linux__)
#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#else /* darwin */
#define AT_FDCWD            (-2)
#define AT_SYMLINK_NOFOLLOW 0x20
#define AT_REMOVEDIR        0x80
#endif
int openat(int, const char *, int, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.openat", C2GO_GOABI0);
#endif
int creat(const char *, mode_t)    /* open() shorthand; source/stdio.c */
    c2go_linkname("github.com/c2gohq/c2go_libc.creat", C2GO_GOABI0);
/* Windows has no fcntl (MinGW-w64 provides none): omit it there. The ported
 * stdio sets close-on-exec / append at open() time and elides fcntl (see
 * source/stdio.c freopen); a Windows caller of fcntl() is a compile error. */
#if !defined(_WIN32)
int fcntl(int, int, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.fcntl", C2GO_GOABI0);
#endif

/* linux-only like the musl source (fallocate(2)); Apple ships no
 * posix_fallocate either — absent on darwin, no fake stub (source/stat2.c).
 * POSIX shape: returns the error number, errno untouched. */
#if defined(__linux__)
int posix_fallocate(int, off_t, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.posix_fallocate", C2GO_GOABI0);
#endif

#endif /* _FCNTL_H */
