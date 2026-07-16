//go:build unix

package libc

// The signal() bridge must manage only ITS OWN os/signal channel: C-side
// SIG_IGN / SIG_DFL transitions must not unhook the Go HOST's own
// signal.Notify subscriptions (the old bridge called the PROCESS-wide
// signal.Ignore / signal.Reset — the timezone-pattern stomp).

import (
	"os"
	"os/signal"
	"syscall"
	"testing"
	"time"
)

const (
	cSIG_DFL = uintptr(0)
	cSIG_IGN = uintptr(1)
)

func waitSig(t *testing.T, ch <-chan os.Signal, what string) {
	t.Helper()
	select {
	case <-ch:
	case <-time.After(3 * time.Second):
		t.Fatalf("host channel did not receive the raise %s — C signal() stomped the host subscription", what)
	}
}

func TestSignalDispositionsKeepHostNotify(t *testing.T) {
	sig := syscall.SIGUSR2
	hostCh := make(chan os.Signal, 4)
	signal.Notify(hostCh, sig)
	defer signal.Stop(hostCh)

	// C sets SIG_IGN: the process must survive the raise (ignored for C), and
	// the HOST subscription must still observe it (no process-wide Ignore).
	Signal(int32(sig), cSIG_IGN)
	if r := Raise(int32(sig)); r != 0 {
		t.Fatalf("Raise = %d, want 0", r)
	}
	waitSig(t, hostCh, "after C SIG_IGN")

	// C restores SIG_DFL: only libc's own channel detaches; the host keeps
	// receiving (no process-wide Reset), and its live subscription is what
	// keeps the default terminate action disabled during this raise.
	Signal(int32(sig), cSIG_DFL)
	if r := Raise(int32(sig)); r != 0 {
		t.Fatalf("Raise = %d, want 0", r)
	}
	waitSig(t, hostCh, "after C SIG_DFL")
}
