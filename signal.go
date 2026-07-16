// signal.go — the Go side of the signal() bridge (source/signal.c). PORTABLE:
// Windows uses the same mechanism (the Go runtime's console ctrl handler feeds
// os/signal: Ctrl+C/Break → SIGINT, console close/logoff/shutdown → SIGTERM);
// only raise() differs (kernel self-signal on unix, synchronous C dispatch on
// Windows — see signal.c).
//
// The C signal() owns the sig->handler table and does the dispatch; Go only
// bridges delivery. For a custom handler the C wrapper asks us to Notify on that
// signal; the pump goroutine then calls back into C (C2goSignalRun, the generated
// binding for __c2go_signal_run) on each delivery, which runs the stored handler
// in C. The handler therefore runs on an ordinary goroutine, never in real
// async-signal context — the honest limit of catching signals under the Go
// runtime, which owns the process's actual handlers.
//
// This bridge manages ONLY ITS OWN channel — never signal.Ignore/signal.Reset,
// which are PROCESS-wide and would also unhook every channel the Go HOST
// registered for that signal (the same stomp-shared-Go-state pattern as the
// timezone bug):
//   - SIG_IGN keeps the pump registered: the runtime consumes deliveries (so
//     e.g. SIGINT no longer kills the process — that IS "ignored"), and the C
//     table drops them (__c2go_signal_run's SIG_IGN check). Caveat: the signal
//     is not os-level SIG_IGN, so an exec'd child does not inherit the ignore.
//   - SIG_DFL detaches only our channel (signal.Stop); os/signal restores the
//     default disposition iff no other channel still watches the signal —
//     exactly right when the host holds its own subscription.

package libc

import (
	"os"
	"os/signal"
	"sync"
	"sync/atomic"
	"syscall"

	_ "unsafe" // for //go:linkname
)

var (
	sigMu    sync.Mutex
	sigPumps = map[int32]chan os.Signal{} // sig -> notify channel while custom-handled or ignored

	// sigHandlers holds the C table's slots as atomics (#658 M13): 0=SIG_DFL,
	// 1=SIG_IGN, else a C handler fp (a code address — integer-legal). signal()
	// swaps, the pump loads: race-free under the Go memory model.
	sigHandlers [33]atomic.Uintptr
)

//go:linkname __c2go_signal_swap
func __c2go_signal_swap(sig int32, h uint64) uint64 {
	if sig <= 0 || int(sig) >= len(sigHandlers) {
		return 0
	}
	return uint64(sigHandlers[sig].Swap(uintptr(h)))
}

//go:linkname __c2go_signal_load
func __c2go_signal_load(sig int32) uint64 {
	if sig <= 0 || int(sig) >= len(sigHandlers) {
		return 0
	}
	return uint64(sigHandlers[sig].Load())
}

// ensurePump registers (once) this signal with os/signal and starts the pump
// goroutine. Used for BOTH a custom handler and SIG_IGN — the C table decides
// per delivery. Caller holds sigMu.
func ensurePump(sig int32) {
	if _, ok := sigPumps[sig]; ok {
		return
	}
	ch := make(chan os.Signal, 1)
	sigPumps[sig] = ch
	signal.Notify(ch, syscall.Signal(sig))
	go func() {
		for range ch {
			// Load-then-run: the table decides per delivery (SIG_DFL/IGN drop).
			if h := sigHandlers[sig].Load(); h > 1 {
				C2goSignalRun(sig, uint64(h))
			}
		}
	}()
}

// stopPump ends any running pump goroutine for sig. Caller holds sigMu. Stop
// drains delivery to the channel before we close it, so no send races the close.
func stopPump(sig int32) {
	if ch, ok := sigPumps[sig]; ok {
		signal.Stop(ch)
		close(ch)
		delete(sigPumps, sig)
	}
}

//go:linkname __c2go_signal_watch
func __c2go_signal_watch(sig int32) {
	sigMu.Lock()
	defer sigMu.Unlock()
	ensurePump(sig)
}

//go:linkname __c2go_signal_ignore
func __c2go_signal_ignore(sig int32) {
	sigMu.Lock()
	defer sigMu.Unlock()
	ensurePump(sig) // pump stays; the C table (SIG_IGN) drops each delivery
}

//go:linkname __c2go_signal_default
func __c2go_signal_default(sig int32) {
	sigMu.Lock()
	defer sigMu.Unlock()
	stopPump(sig) // ONLY our channel; host subscriptions (if any) keep working
}
