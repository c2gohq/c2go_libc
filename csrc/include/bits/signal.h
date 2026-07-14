#ifndef _BITS_SIGNAL_H
#define _BITS_SIGNAL_H

/* c2go-libc: per-OS signal numbers — three distinct namespaces (Linux=musl
 * generic, macOS=Darwin/BSD, Windows=MinGW CRT). The macro name is the
 * portable interface; the number is the OS's own. The Go os/signal bridge
 * delivers what it can and rejects the rest. See ../PORTABILITY.md. */

#if defined(__linux__)
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   29
#define SIGPWR    30
#define SIGSYS    31

#elif defined(__APPLE__)
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGEMT     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGBUS    10
#define SIGSEGV   11
#define SIGSYS    12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGURG    16
#define SIGSTOP   17
#define SIGTSTP   18
#define SIGCONT   19
#define SIGCHLD   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGIO     23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGUSR1   30
#define SIGUSR2   31

#elif defined(_WIN32)
/* The Windows CRT delivers only these six signals. */
#define SIGINT     2
#define SIGILL     4
#define SIGFPE     8
#define SIGSEGV   11
#define SIGTERM   15
#define SIGABRT   22

#else
#error "unsupported c2go OS"
#endif

#endif /* _BITS_SIGNAL_H */
