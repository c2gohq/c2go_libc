// popen_unix.go — #678: the unix per-OS points of popen/pclose.
//go:build unix

package libc

import (
	"os"
	"os/exec"
	"syscall"
)

func popenShell(command string) *exec.Cmd {
	return exec.Command("/bin/sh", "-c", command)
}

// cPipeFd hands the pipe end to the C world: dup a C-exclusive fd (unix fds
// ARE kernel fds) and close Go's copy so C's fclose is the only closer.
func cPipeFd(pipe *os.File) (int32, error) {
	fd, err := syscall.Dup(int(pipe.Fd()))
	pipe.Close()
	if err != nil {
		return -1, err
	}
	fd, err = c2goFD(fd)
	if err != nil {
		return -1, err
	}
	return int32(fd), nil
}

// pcloseStatus: musl pclose returns the raw waitpid status (the sys/wait.h
// macros decode it).
func pcloseStatus(ee *exec.ExitError) int64 {
	if ws, ok := ee.Sys().(syscall.WaitStatus); ok {
		return int64(int32(ws))
	}
	return int64(ee.ExitCode()) << 8
}
