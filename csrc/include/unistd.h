/* unistd.h — POSIX API: file descriptors, paths, process ids. */
#ifndef _UNISTD_H
#define _UNISTD_H

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_gid_t
#include <bits/alltypes.h>
#include <c2go.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* file-descriptor I/O — the implementation is a C wrapper (source/io_posix.c on
 * Unix, source/io_windows.c on Windows) reached straight through the
 * c2go_linkname; the linked symbol is the same lowercase C name on both. On Unix
 * the wrapper composes a Go syscall shim (io.go, __c2go_syscall_*); on Windows it
 * calls the CRT fd table (msvcrt _open/_read/…). */
ssize_t read(int, void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.read", C2GO_GOABI0);
ssize_t write(int, const void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.write", C2GO_GOABI0);
off_t   lseek(int, off_t, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.lseek", C2GO_GOABI0);
int     close(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.close", C2GO_GOABI0);
int     dup2(int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.dup2", C2GO_GOABI0);
/* dup3 is Unix-only: MinGW-w64 has none (a Linux syscall); freopen uses dup2 */
#if !defined(_WIN32)
int     dup3(int, int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.dup3", C2GO_GOABI0);
#endif
ssize_t pread(int, void *, size_t, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.pread", C2GO_GOABI0);
ssize_t pwrite(int, const void *, size_t, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.pwrite", C2GO_GOABI0);
int     dup(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.dup", C2GO_GOABI0);
int     pipe(int [2])
    c2go_linkname("github.com/c2gohq/c2go_libc.pipe", C2GO_GOABI0);
int     fsync(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fsync", C2GO_GOABI0);
int     fdatasync(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fdatasync", C2GO_GOABI0);
int     ftruncate(int, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.ftruncate", C2GO_GOABI0);
int     truncate(const char *, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.truncate", C2GO_GOABI0);
int     isatty(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.isatty", C2GO_GOABI0);

/* paths */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
int      access(const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.access", C2GO_GOABI0);
/* unlink/rmdir: like the fd ops above, a package-provided C wrapper reached
 * through the same lowercase symbol — over a Go syscall shim on Unix
 * (source/io_posix.c), over msvcrt on Windows (source/io_windows.c). remove()
 * (<stdio.h>) is layered on both. */
int      unlink(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.unlink", C2GO_GOABI0);
int      rmdir(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rmdir", C2GO_GOABI0);
int      link(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.link", C2GO_GOABI0);
int      symlink(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.symlink", C2GO_GOABI0);
ssize_t  readlink(const char *__restrict, char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.readlink", C2GO_GOABI0);
int      chdir(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.chdir", C2GO_GOABI0);
char    *getcwd(char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.getcwd", C2GO_GOABI0);

/* getopt (POSIX). getopt_long / struct option live in <getopt.h>. The state
 * globals are shared C globals (source/getopt.c) the caller reads. */
int getopt(int, char *const [], const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getopt", C2GO_GOABI0);
extern char *optarg;
extern int optind, opterr, optopt;

/* environ (#675, source/env.c): a rebuilt snapshot of the os-owned process
 * environment — refreshed by every setenv/unsetenv/putenv/clearenv through
 * this libc. Read idiom only; direct writes into the array do not propagate
 * back (see env.c's header for the recorded deviations). */
extern char **environ;

/* #675 stage C — unix fd/path surface (source/unistd2.c over Go syscall
 * shims; MinGW has none of these, so they are unix-only). unlinkat/renameat
 * per POSIX placement; readv/writev live in <sys/uio.h>, openat in
 * <fcntl.h>, mkdirat in <sys/stat.h>. */
#if !defined(_WIN32)
int fchdir(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fchdir", C2GO_GOABI0);
int chown(const char *, uid_t, gid_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.chown", C2GO_GOABI0);
int fchown(int, uid_t, gid_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.fchown", C2GO_GOABI0);
int lchown(const char *, uid_t, gid_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.lchown", C2GO_GOABI0);
/* pipe itself predates this batch (fsops_posix.c, declared above). */
int pipe2(int [2], int)
    c2go_linkname("github.com/c2gohq/c2go_libc.pipe2", C2GO_GOABI0);
void sync(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.sync", C2GO_GOABI0);
int unlinkat(int, const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.unlinkat", C2GO_GOABI0);
int renameat(int, const char *, int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.renameat", C2GO_GOABI0);
/* #675 stage D — controlling-terminal process group (source/termios.c over
 * the whitelisted TIOCGPGRP/TIOCSPGRP ioctl; POSIX places them here). */
pid_t tcgetpgrp(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcgetpgrp", C2GO_GOABI0);
int tcsetpgrp(int, pid_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.tcsetpgrp", C2GO_GOABI0);
#endif /* !_WIN32 */

/* conf surface (#675 "1+3" decision): cross-platform — backed by
 * platform-independent Go primitives (source/conf.c). gethostname is in
 * MinGW anyway (winsock); the other three are provided everywhere under the
 * relaxed "Go primitive honestly delivers the semantics" line. */
int gethostname(char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.gethostname", C2GO_GOABI0);
int getentropy(void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.getentropy", C2GO_GOABI0);
int getpagesize(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.getpagesize", C2GO_GOABI0);
/* sysconf: whitelist only (musl name values) — everything else EINVAL. */
#define _SC_CLK_TCK          2
#define _SC_PAGESIZE         30
#define _SC_PAGE_SIZE        30
#define _SC_NPROCESSORS_ONLN 84
long sysconf(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.sysconf", C2GO_GOABI0);

/* time / process. sleep/usleep are C wrappers over nanosleep (source/time.c);
 * getpid is a cross-platform Go bridge (process.go); getppid + the uid/gid family
 * are Unix-only Go bridges (process_unix.go, no MinGW equivalent); _exit is a raw
 * linkname to the same __c2go_exit termination bridge that _Exit uses. */
unsigned sleep(unsigned)
    c2go_linkname("github.com/c2gohq/c2go_libc.sleep", C2GO_GOABI0);
int      usleep(unsigned)
    c2go_linkname("github.com/c2gohq/c2go_libc.usleep", C2GO_GOABI0);
/* alarm / pause are intentionally absent: both rely on POSIX signal-timer
 * delivery (SIGALRM), which c2go's signal model does not provide (handlers live
 * only in the C world). A bare prototype with no backing symbol is a link-time
 * lie, so they are omitted — a caller becomes a compile error. */
pid_t    getpid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getpid", C2GO_GOABI0);
pid_t    getppid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getppid", C2GO_GOABI0);
uid_t    getuid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getuid", C2GO_GOABI0);
uid_t    geteuid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Geteuid", C2GO_GOABI0);
gid_t    getgid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getgid", C2GO_GOABI0);
gid_t    getegid(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getegid", C2GO_GOABI0);
_Noreturn void _exit(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_exit", C2GO_GOABI0);

/* NOTE: fork() / exec*() are intentionally omitted — they cannot run safely
 * under the Go runtime (OS threads + GC). */


#if !defined(_WIN32)
/* lockf (#664, musl misc/lockf.c): a thin veneer over the fcntl record locks
 * (#648). Constants are musl's. */
#define F_ULOCK 0
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3
int lockf(int, int, off_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.lockf", C2GO_GOABI0);
#endif

/* byte-pair swap (musl string/swab.c — pure computation, cross-platform;
 * POSIX places the declaration here). Impl source/string.c. */
void swab(const void *__restrict, void *__restrict, ssize_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.swab", C2GO_GOABI0);

#endif /* _UNISTD_H */
