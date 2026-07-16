//go:build unix

package selftest

import (
	"bytes"
	"os"
	"os/exec"
	"strings"
	"testing"

	// Force-link c2go-libc so its atexit/exit/write/__assert_fail symbols
	// (referenced cross-package by the selftest .s) are present.
	_ "github.com/c2gohq/c2go_libc"
)

// TestAtexitLIFO verifies atexit() handlers fire in LIFO (reverse registration)
// order. exit()/atexit() call os.Exit, so they cannot run in-process; the test
// re-execs this binary as a helper. The child registers three handlers that
// each write one byte straight to fd 1 (write() is an unbuffered syscall) and
// calls exit(0), which drains the registry back-to-front. exit(0) → os.Exit(0)
// halts before the test framework prints anything of its own, so the child's
// stdout is exactly the handler output — "321" iff ordering is LIFO.
func TestAtexitLIFO(t *testing.T) {
	if os.Getenv("C2GO_ATEXIT_HELPER") == "lifo" {
		AtexitSelftestLIFO() // registers h1,h2,h3; exit(0) runs them LIFO
		return               // unreachable: exit(0) already halted the process
	}

	cmd := exec.Command(os.Args[0], "-test.run=^TestAtexitLIFO$")
	cmd.Env = append(os.Environ(), "C2GO_ATEXIT_HELPER=lifo")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	if err := cmd.Run(); err != nil {
		t.Fatalf("LIFO helper run error: %v (stdout=%q)", err, stdout.String())
	}
	if got := stdout.String(); got != "321" {
		t.Fatalf("atexit firing order = %q, want %q (handlers must run LIFO)", got, "321")
	}
}

// TestAssertMessage verifies __assert_fail prints the standard diagnostic to
// stderr and terminates via abort(). __assert_fail → abort → os.Exit(134), so
// it too runs in a re-exec'd child. The child fails assert(x == 42) (x is 0);
// stderr is unbuffered, so the message reaches fd 2 before abort() stops the
// process. The parent asserts the message and that the exit code is 134
// (128 + SIGABRT).
func TestAssertMessage(t *testing.T) {
	if os.Getenv("C2GO_ASSERT_HELPER") == "fail" {
		AtexitSelftestAssert() // assert fails → __assert_fail → abort
		return                 // unreachable: abort() already halted the process
	}

	cmd := exec.Command(os.Args[0], "-test.run=^TestAssertMessage$")
	cmd.Env = append(os.Environ(), "C2GO_ASSERT_HELPER=fail")
	var stderr bytes.Buffer
	cmd.Stderr = &stderr
	err := cmd.Run()

	ee, ok := err.(*exec.ExitError)
	if !ok {
		t.Fatalf("assert helper: expected abort to fail the process, got err=%v (stderr=%q)",
			err, stderr.String())
	}
	if code := ee.ExitCode(); code != 134 {
		t.Fatalf("assert exit code = %d, want 134 (128+SIGABRT via abort)", code)
	}
	if got := stderr.String(); !strings.Contains(got, "Assertion failed: x == 42") {
		t.Fatalf("assert stderr = %q, want it to contain %q", got, "Assertion failed: x == 42")
	}
}
