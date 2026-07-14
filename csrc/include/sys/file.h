/* sys/file.h — BSD flock() (#648, Unix only: MinGW-w64 has none, and a fake
 * stub would lie). The operation bits are identical on linux and macOS, so
 * they pass through natively (../flock.go over syscall.Flock). flock locks
 * the OPEN FILE DESCRIPTION (two open()s of the same file DO conflict, even
 * in one process) — unlike fcntl record locks, which are per-process. */
#ifndef _SYS_FILE_H
#define _SYS_FILE_H

#include <c2go.h>

#if !defined(_WIN32)

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

int flock(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.flock", C2GO_GOABI0);

#endif /* !_WIN32 */

#endif /* _SYS_FILE_H */
