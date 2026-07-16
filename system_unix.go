// system_unix.go — system(3) over the Go os/exec bridge (Unix).
//
// The C libc omits fork()/exec*() because they cannot run safely under the Go
// runtime, so system() cannot fork/exec directly. Instead it spawns the command
// through os/exec — the runtime-safe way to start a child — running it under
// "/bin/sh -c" exactly like musl's system(). The shell inherits the caller's
// stdin/stdout/stderr, so its output interleaves with the program's own.
//
// The return value follows musl: on a normal run it is the raw wait status of
// the shell (so `system(cmd) == 0` still tests success, and `(rc >> 8) & 0xff`
// recovers the exit code — this libc intentionally has no WEXITSTATUS macro).
// A NULL command returns nonzero (a command interpreter is always available:
// /bin/sh). If /bin/sh could not be started at all, it returns -1.
//
// Note: system() does NOT flush C's buffered stdio first (musl does not either);
// interleaving with buffered C stdout is the caller's responsibility.
//go:build unix

package libc

import (
	"os"
	"os/exec"
	"syscall"
	_ "unsafe" // for //go:linkname
)

//go:linkname System
func System(command *byte) int32 {
	if command == nil {
		return 1 // POSIX: nonzero if a command interpreter is available; /bin/sh always is
	}
	cmd := exec.Command("/bin/sh", "-c", cstr(command))
	cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, os.Stdout, os.Stderr
	err := cmd.Run()
	if err == nil {
		return 0
	}
	if ee, ok := err.(*exec.ExitError); ok {
		if ws, ok := ee.Sys().(syscall.WaitStatus); ok {
			return int32(ws) // raw wait status (musl-faithful)
		}
		return int32(ee.ExitCode()) << 8 // fallback: synthesize an exited status
	}
	return -1 // could not start /bin/sh (fork/exec-equivalent failure)
}
