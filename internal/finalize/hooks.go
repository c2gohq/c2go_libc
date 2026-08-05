// SPDX-License-Identifier: AGPL-3.0-only

// Package finalize coordinates package-local cleanup that must run from the
// C2Go C exit path. It is internal so registering a hook does not become part
// of libc's public compatibility surface.
package finalize

import "sync"

var (
	hooksMu sync.Mutex
	runMu   sync.Mutex
	hooks   []func()
)

// Register adds a cleanup hook. Packages should register during init; a hook
// must be safe to run more than once because C2goFinalize is idempotent and may
// be called explicitly before a later C exit.
func Register(hook func()) {
	if hook == nil {
		panic("c2go-libc: nil finalize hook")
	}
	hooksMu.Lock()
	hooks = append(hooks, hook)
	hooksMu.Unlock()
}

// Run invokes a stable snapshot of the registered hooks. The hook list remains
// installed so resources created after an explicit C2goFinalize call are also
// flushed by a later call.
func Run() {
	runMu.Lock()
	defer runMu.Unlock()

	hooksMu.Lock()
	snapshot := append([]func(){}, hooks...)
	hooksMu.Unlock()

	for _, hook := range snapshot {
		hook()
	}
}
