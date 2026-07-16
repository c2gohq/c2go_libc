// system_windows.go — system(3) over the Go os/exec bridge (Windows).
//
// Mirrors system_unix.go but runs the command through "cmd /c", matching
// msvcrt's system(). Windows has no Unix-style wait status, so the return value
// is the command's exit code directly (as msvcrt does): 0 on success, the child
// exit code on failure, -1 if cmd.exe could not be started, and nonzero for a
// NULL command (a command interpreter is always available).
//go:build windows

package libc

import (
	"os"
	"os/exec"
	_ "unsafe" // for //go:linkname
)

//go:linkname System
func System(command *byte) int32 {
	if command == nil {
		return 1 // a command interpreter (cmd.exe) is always available
	}
	cmd := exec.Command("cmd", "/c", cstr(command))
	cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, os.Stdout, os.Stderr
	err := cmd.Run()
	if err == nil {
		return 0
	}
	if ee, ok := err.(*exec.ExitError); ok {
		return int32(ee.ExitCode()) // msvcrt returns the command's exit code
	}
	return -1 // could not start cmd.exe
}
