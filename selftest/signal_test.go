//go:build unix

package selftest

import (
	"bytes"
	"os"
	"os/exec"
	"syscall"
	"testing"
	"time"

	// Force-link c2go-libc so its signal()/raise() symbols (referenced
	// cross-package by the selftest .s) are present.
	_ "github.com/c2gohq/c2go_libc"
)

// TestSignal drives the in-C signal()/raise() exercise
// (selftest/source/signal_selftest.c): a custom SIGUSR1 handler fires on a
// self-raise, SIGSEGV is honestly refused (SIG_ERR), and a SIG_IGN install
// reports the prior SIG_DFL.
func TestSignal(t *testing.T) {
	if SignalSelftestSegvRefused() != 1 {
		t.Fatal("signal(SIGSEGV, h) must return SIG_ERR with errno EINVAL")
	}
	if SignalSelftestIgn() != 1 {
		t.Fatal("signal(SIGUSR2, SIG_IGN) must return SIG_DFL as the old handler")
	}
	SignalSelftestInstall()
	if r := SignalSelftestRaise(); r != 0 {
		t.Fatalf("raise(SIGUSR1) returned %d, want 0", r)
	}
	// The handler runs on the os/signal pump goroutine, so poll for delivery.
	deadline := time.Now().Add(3 * time.Second)
	for SignalSelftestFired() == 0 {
		if time.Now().After(deadline) {
			t.Fatal("SIGUSR1 handler did not fire within 3s")
		}
		time.Sleep(2 * time.Millisecond)
	}
	if got, want := SignalSelftestFired(), int32(syscall.SIGUSR1); got != want {
		t.Fatalf("handler saw signal %d, want SIGUSR1 (%d)", got, want)
	}
}

// TestAbortRunsHandler: abort() must run a caught SIGABRT handler ONCE,
// synchronously, before terminating (POSIX). abort() halts the process, so the
// exercise runs in a re-exec'd child (the TestAtexitLIFO pattern): the child
// installs a handler that writes "H" to fd 1 and calls abort(); the parent
// asserts the byte arrived AND the process still died with the abort status.
func TestAbortRunsHandler(t *testing.T) {
	if os.Getenv("C2GO_ABORT_HELPER") == "run" {
		SignalSelftestAbort() // installs the handler, abort()s
		return                // unreachable: abort() already halted the process
	}

	cmd := exec.Command(os.Args[0], "-test.run=^TestAbortRunsHandler$")
	cmd.Env = append(os.Environ(), "C2GO_ABORT_HELPER=run")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	err := cmd.Run()

	ee, ok := err.(*exec.ExitError)
	if !ok {
		t.Fatalf("abort helper did not exit with an error status (err=%v, stdout=%q)", err, stdout.String())
	}
	if code := ee.ExitCode(); code != 134 {
		t.Fatalf("abort helper exit code = %d, want 134 (128+SIGABRT)", code)
	}
	if got := stdout.String(); got != "H" {
		t.Fatalf("SIGABRT handler output = %q, want %q (abort must run the caught handler once)", got, "H")
	}
}
