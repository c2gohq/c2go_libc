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
	"unsafe"
)

var popenTab handleTable[exec.Cmd]

// popenStart owns the common process/pipe setup for both public carriers. The
// root libc wrapper stores the returned command in popenTab because its FILE is
// unscanned C memory. mlib instead stores the direct pointer in its typed
// gc_malloc FILE carrier, so it does not need a second process table.
func popenStart(command *byte, write int32, fdout *int32) (*exec.Cmd, int64) {
	cmd := popenShell(cstr(command))
	var pipe *os.File
	if write != 0 {
		w, err := cmd.StdinPipe()
		if err != nil {
			return nil, -int64(syscall.EMFILE)
		}
		pipe = w.(*os.File)
		cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
	} else {
		r, err := cmd.StdoutPipe()
		if err != nil {
			return nil, -int64(syscall.EMFILE)
		}
		pipe = r.(*os.File)
		cmd.Stdin, cmd.Stderr = os.Stdin, os.Stderr
	}
	if err := cmd.Start(); err != nil {
		pipe.Close()
		return nil, -int64(syscall.EAGAIN) // could not spawn the shell
	}
	fd, err := cPipeFd(pipe) // C-exclusive fd (per-OS); Go's copy closes inside
	if err != nil {
		cmd.Process.Kill()
		cmd.Wait()
		return nil, -int64(errnoOf(err))
	}
	*fdout = fd
	return cmd, 0
}

func popenWait(cmd *exec.Cmd) int64 {
	err := cmd.Wait()
	if err == nil {
		return 0
	}
	if ee, ok := err.(*exec.ExitError); ok {
		return pcloseStatus(ee) // per-OS: unix raw wait status; windows exit code (_pclose shape)
	}
	return -int64(syscall.ECHILD)
}

// __c2go_popen starts the platform shell with one pipe end (write!=0: child
// stdin, else child stdout). Returns the handle id (>=1) and stores the C-owned
// pipe fd in *fdout, or returns -errno.
//
//go:linkname __c2go_popen
func __c2go_popen(command *byte, write int32, fdout *int32) int64 {
	cmd, result := popenStart(command, write, fdout)
	if result < 0 {
		return result
	}
	return int64(popenTab.alloc(cmd))
}

// __c2go_popen_managed returns the direct *exec.Cmd pointer used by mlib's
// typed FILE carrier. The pointer may only be retained in GC-visible managed
// storage; root libc must continue to use __c2go_popen and popenTab.
//
//go:linkname __c2go_popen_managed
func __c2go_popen_managed(command *byte, write int32, fdout, errorOut *int32) unsafe.Pointer {
	cmd, result := popenStart(command, write, fdout)
	if result < 0 {
		*errorOut = int32(-result)
		return nil
	}
	*errorOut = 0
	return unsafe.Pointer(cmd)
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
	return popenWait(cmd)
}

// __c2go_pclose_managed waits for a command retained directly by mlib. The C
// caller closes the pipe before entering this function, exactly as root
// pclose does. A managed local keeps the command live while Wait runs.
//
//go:linkname __c2go_pclose_managed
func __c2go_pclose_managed(process unsafe.Pointer) int64 {
	if process == nil {
		return -int64(syscall.EINVAL)
	}
	return popenWait((*exec.Cmd)(process))
}
