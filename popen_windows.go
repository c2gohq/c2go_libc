// popen_windows.go — #678: the windows per-OS points of popen/pclose.
// MinGW's reference is _popen/_pclose (stdio.h + the OLDNAMES alias): shell =
// cmd /c, and pclose returns the command's EXIT CODE (not a unix wait
// status — sys/wait.h stays #error-guarded on windows per #662).
//go:build windows

package libc

import (
	"os"
	"os/exec"
	"syscall"
)

func popenShell(command string) *exec.Cmd {
	return exec.Command("cmd", "/c", command)
}

// cPipeFd hands the pipe end to the C world. os.File.Fd() is a HANDLE, not a
// CRT fd (the #677 audit's core windows pitfall), so the C-world fd is made
// by DuplicateHandle (own copy, non-inheritable) + msvcrt _open_osfhandle
// (HANDLE -> CRT fd); Go's *os.File then closes, leaving C's fclose as the
// only closer of the duplicated handle.
func cPipeFd(pipe *os.File) (int32, error) {
	cur, _ := syscall.GetCurrentProcess()
	var dup syscall.Handle
	err := syscall.DuplicateHandle(cur, syscall.Handle(pipe.Fd()), cur, &dup,
		0, false, syscall.DUPLICATE_SAME_ACCESS)
	pipe.Close()
	if err != nil {
		return -1, err
	}
	const _O_BINARY = 0x8000
	fd, _, _ := msvcrtOpenOsfhandle.Call(uintptr(dup), _O_BINARY)
	if int32(fd) < 0 {
		syscall.CloseHandle(dup)
		return -1, syscall.EMFILE
	}
	return int32(fd), nil
}

var msvcrtOpenOsfhandle = syscall.NewLazyDLL("msvcrt.dll").NewProc("_open_osfhandle")

// pcloseStatus: _pclose returns the command's exit code directly.
func pcloseStatus(ee *exec.ExitError) int64 {
	return int64(ee.ExitCode())
}
