/* strsignal.c — split out of the OLD string.c bucket (the rest of that
 * bucket is fork-per-file musl now). Per-OS ADAPTED, see the note below. */
#include <signal.h>
#include <string.h>
#include <c2go.h>

/* musl src/string/strsignal.c. The compile-time "linux numbering" fast path
 * collapses on linux (sigmap is identity) and the map[] branch handles
 * darwin's BSD numbering — both from the target's native <signal.h> values.
 * Unix-only: windows' signal.h has only the six ANSI signals and MinGW
 * ships no strsignal (#677 line). Deltas: _NSIG is local per-OS (our
 * signal.h does not export it); musl's LCTRANS_CUR message translation is
 * identity (single C locale). */
#if !defined(_WIN32)
#include <signal.h>

#if defined(__APPLE__)
#define C2GO_NSIG 32
#else
#define C2GO_NSIG 65
#endif

#if (SIGHUP == 1) && (SIGINT == 2) && (SIGQUIT == 3) && (SIGILL == 4) \
 && (SIGTRAP == 5) && (SIGABRT == 6) && (SIGBUS == 7) && (SIGFPE == 8) \
 && (SIGKILL == 9) && (SIGUSR1 == 10) && (SIGSEGV == 11) && (SIGUSR2 == 12) \
 && (SIGPIPE == 13) && (SIGALRM == 14) && (SIGTERM == 15) \
 && (SIGCHLD == 17) && (SIGCONT == 18) && (SIGSTOP == 19) && (SIGTSTP == 20) \
 && (SIGTTIN == 21) && (SIGTTOU == 22) && (SIGURG == 23) && (SIGXCPU == 24) \
 && (SIGXFSZ == 25) && (SIGVTALRM == 26) && (SIGPROF == 27) && (SIGWINCH == 28) \
 && (SIGSYS == 31)

#define sigmap(x) (x)

#else

static const char sigmap_tbl[] = {
	[SIGHUP]    = 1,
	[SIGINT]    = 2,
	[SIGQUIT]   = 3,
	[SIGILL]    = 4,
	[SIGTRAP]   = 5,
	[SIGABRT]   = 6,
	[SIGBUS]    = 7,
	[SIGFPE]    = 8,
	[SIGKILL]   = 9,
	[SIGUSR1]   = 10,
	[SIGSEGV]   = 11,
	[SIGUSR2]   = 12,
	[SIGPIPE]   = 13,
	[SIGALRM]   = 14,
	[SIGTERM]   = 15,
#if defined(SIGEMT)
	[SIGEMT]    = 16,
#endif
	[SIGCHLD]   = 17,
	[SIGCONT]   = 18,
	[SIGSTOP]   = 19,
	[SIGTSTP]   = 20,
	[SIGTTIN]   = 21,
	[SIGTTOU]   = 22,
	[SIGURG]    = 23,
	[SIGXCPU]   = 24,
	[SIGXFSZ]   = 25,
	[SIGVTALRM] = 26,
	[SIGPROF]   = 27,
	[SIGWINCH]  = 28,
	[SIGSYS]    = 31
};

#define sigmap(x) ((size_t)(x) >= sizeof sigmap_tbl ? (x) : sigmap_tbl[(x)])

#endif

static const char sigstrings[] =
	"Unknown signal\0"
	"Hangup\0"
	"Interrupt\0"
	"Quit\0"
	"Illegal instruction\0"
	"Trace/breakpoint trap\0"
	"Aborted\0"
	"Bus error\0"
	"Arithmetic exception\0"
	"Killed\0"
	"User defined signal 1\0"
	"Segmentation fault\0"
	"User defined signal 2\0"
	"Broken pipe\0"
	"Alarm clock\0"
	"Terminated\0"
#if defined(SIGEMT)
	"Emulator trap\0"
#else
	"Stack fault\0"
#endif
	"Child process status\0"
	"Continued\0"
	"Stopped (signal)\0"
	"Stopped\0"
	"Stopped (tty input)\0"
	"Stopped (tty output)\0"
	"Urgent I/O condition\0"
	"CPU time limit exceeded\0"
	"File size limit exceeded\0"
	"Virtual timer expired\0"
	"Profiling timer expired\0"
	"Window changed\0"
	"I/O possible\0"
	"Power failure\0"
	"Bad system call\0"
	"RT32"
	"\0RT33\0RT34\0RT35\0RT36\0RT37\0RT38\0RT39\0RT40"
	"\0RT41\0RT42\0RT43\0RT44\0RT45\0RT46\0RT47\0RT48"
	"\0RT49\0RT50\0RT51\0RT52\0RT53\0RT54\0RT55\0RT56"
	"\0RT57\0RT58\0RT59\0RT60\0RT61\0RT62\0RT63\0RT64"
	"";

c2go_extern char *strsignal(int signum)
{
	const char *s = sigstrings;

	signum = sigmap(signum);
	if (signum - 1U >= C2GO_NSIG-1) signum = 0;

	for (; signum--; s++) for (; *s; s++);

	return (char *)s;
}
#endif /* !_WIN32 (strsignal) */
