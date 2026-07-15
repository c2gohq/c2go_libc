// popen.go — popen/pclose (#662, cross-platform since #678): the second
// process-spawn surface after system(). musl popen.c is fork+dup2+FILE; under
// the Go runtime the spawn rides os/exec (the same genuine-primitive
// composition as system_unix.go), and the C side wraps the pipe fd with
// fdopen — musl's own shape. MinGW HAS popen (_popen + the OLDNAMES alias),
// so windows is a reference-backed target, not an extension.
//
// fd ownership is made EXCLUSIVE to the C world: the pipe end is duplicated
// into a C-world fd (unix: dup; windows: DuplicateHandle + _open_osfhandle,
// since os.File.Fd() is a HANDLE, not a CRT fd) and the Go *os.File closed
// immediately, so C's fclose is the only closer. popenTab roots the
// *exec.Cmd (generation ids, #658) until pclose Wait()s it. The per-OS
// points (shell command line, fd handoff, wait-status encoding) live in
// popen_unix.go / popen_windows.go.

package libc

import (
	"os"
	"os/exec"
	"syscall"

	_ "unsafe" // for //go:linkname
)

var popenTab handleTable[exec.Cmd]

// __c2go_popen starts the platform shell with one pipe end (write!=0: child
// stdin, else child stdout). Returns the handle id (>=1) and stores the C-owned
// pipe fd in *fdout, or returns -errno.
//
//go:linkname __c2go_popen
func __c2go_popen(command *byte, write int32, fdout *int32) int64 {
	cmd := popenShell(cstr(command))
	var pipe *os.File
	if write != 0 {
		w, err := cmd.StdinPipe()
		if err != nil {
			return -int64(syscall.EMFILE)
		}
		pipe = w.(*os.File)
		cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
	} else {
		r, err := cmd.StdoutPipe()
		if err != nil {
			return -int64(syscall.EMFILE)
		}
		pipe = r.(*os.File)
		cmd.Stdin, cmd.Stderr = os.Stdin, os.Stderr
	}
	if err := cmd.Start(); err != nil {
		pipe.Close()
		return -int64(syscall.EAGAIN) // could not spawn the shell
	}
	fd, err := cPipeFd(pipe) // C-exclusive fd (per-OS); Go's copy closes inside
	if err != nil {
		cmd.Process.Kill()
		cmd.Wait()
		return -int64(errnoOf(err))
	}
	*fdout = fd
	return int64(popenTab.alloc(cmd))
}

// __c2go_pclose Wait()s the child and returns its raw wait status (system()'s
// encoding — the sys/wait.h macros decode it), or -errno for a stale handle.
// The C side has already fclose()'d the pipe (child sees EOF / SIGPIPE).
//
//go:linkname __c2go_pclose
func __c2go_pclose(id int64) int64 {
	cmd := popenTab.get(uint64(id))
	if cmd == nil {
		return -int64(syscall.EINVAL)
	}
	popenTab.free(uint64(id))
	err := cmd.Wait()
	if err == nil {
		return 0
	}
	if ee, ok := err.(*exec.ExitError); ok {
		return pcloseStatus(ee) // per-OS: unix raw wait status; windows exit code (_pclose shape)
	}
	return -int64(syscall.ECHILD)
}
