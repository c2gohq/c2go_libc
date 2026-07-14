/* sys/wait.h — wait-status decoding macros only (#654: luaL_execresult needs
 * them to decode system()'s return). musl bits shape; the wait functions
 * themselves are NOT provided (no fork/wait under the Go runtime — system()
 * is the process-spawn surface, see system_unix.go). */
#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H
#if defined(_WIN32)
#error "sys/wait.h: no such header on Windows (MinGW has none); system()'s status is unix-encoded only on unix targets"
#endif
#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WSTOPSIG(s)    WEXITSTATUS(s)
#define WIFEXITED(s)   (!WTERMSIG(s))
#define WIFSTOPPED(s)  ((short)(((s)&0xffff)*0x10001U) >> 8 > 0x7f00)
#define WIFSIGNALED(s) (((s)&0xffff)-1U < 0xffu)
#endif
