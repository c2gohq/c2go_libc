// process.go — process-control and environment bridges (cross-platform).
//
// exit/atexit and the atexit handler registry live in C (source/stdlib.c) so the
// registered C function pointers are invoked with a native call; only the final
// process termination crosses into Go here, because os.Exit is the sole way to
// stop the Go runtime. getenv/setenv/unsetenv are implemented directly in Go over
// the os package, which owns the process environment under the Go runtime.
package libc

import (
	"os"
	"runtime"
	"sync"
	"unsafe" // unsafe.Slice for the environ snapshot; also //go:linkname
)

// __c2go_exit terminates the process after C's exit() has run the atexit handlers
// and flushed stdio. os.Exit does not run Go defers, matching C exit semantics.
//go:linkname __c2go_exit
func __c2go_exit(code int32) { os.Exit(int(code)) }

// __c2go_abort implements abort(). The Go runtime offers no SIGABRT/core path, so
// terminate with the conventional 128+SIGABRT(6) status to signal abnormal exit.
//go:linkname __c2go_abort
func __c2go_abort() { os.Exit(134) }

// atexitMu serialises the atexit / at_quick_exit registries (source/stdlib.c).
// A minimal libc leaves them unlocked ("registered before threads start"), but
// under the Go runtime any goroutine may call atexit concurrently, so the bare
// __atexit_n++ was a data race. C pops each handler under this lock and runs it
// unlocked, so a handler may safely re-register.
var atexitMu sync.Mutex

//go:linkname _c2go_atexit_lock
func _c2go_atexit_lock() { atexitMu.Lock() }

//go:linkname _c2go_atexit_unlock
func _c2go_atexit_unlock() { atexitMu.Unlock() }

// Getpid implements getpid() (pid_t == int32). os.Getpid is cross-platform, so it
// lives here rather than in the Unix-only process_unix.go.
//go:linkname Getpid
func Getpid() int32 { return int32(os.Getpid()) }

// __c2go_progname backs err.c's lazy __progname (#675): the basename of
// os.Args[0], cached as a NUL-terminated Go-heap buffer (stable pointer,
// same rooting model as envCache).
var prognameBuf []byte

//go:linkname __c2go_progname
func __c2go_progname() *byte {
	envMu.Lock()
	defer envMu.Unlock()
	if prognameBuf == nil {
		n := "?"
		if len(os.Args) > 0 && os.Args[0] != "" {
			n = os.Args[0]
			for i := len(n) - 1; i >= 0; i-- {
				if n[i] == '/' || n[i] == '\\' {
					n = n[i+1:]
					break
				}
			}
		}
		prognameBuf = append([]byte(n), 0)
	}
	return &prognameBuf[0]
}

// env — getenv returns a C string that stays valid until the next getenv/setenv
// of the SAME name (the C guarantee). Each lookup caches a fresh NUL-terminated
// copy in envCache, which roots it on the Go heap (GC never moves it), replacing
// any earlier copy for that name.
var (
	envMu    sync.Mutex
	envCache = map[string][]byte{}
)

//go:linkname Getenv
func Getenv(name *byte) *byte {
	n := cstr(name)
	v, ok := os.LookupEnv(n)
	if !ok {
		return nil
	}
	envMu.Lock()
	defer envMu.Unlock()
	// Reuse the cached buffer when the value is unchanged, so repeated getenv of the
	// same variable returns a STABLE pointer (glibc-like) instead of churning a new
	// []byte each call and dropping the previous one's GC root — which, if the C
	// side still held that earlier pointer, let the backing buffer be collected
	// (dangling). Only a genuine value change replaces it, at which point the prior
	// pointer is invalidated per the getenv contract anyway.
	if b, ok := envCache[n]; ok && len(b) == len(v)+1 && string(b[:len(v)]) == v {
		return &b[0]
	}
	b := append([]byte(v), 0)
	envCache[n] = b
	return &b[0]
}

//go:linkname Setenv
func Setenv(name, value *byte, overwrite int32) int32 {
	n := cstr(name)
	if overwrite == 0 {
		if _, ok := os.LookupEnv(n); ok {
			return 0 // exists and overwrite==0: leave it, success
		}
	}
	if os.Setenv(n, cstr(value)) != nil {
		setErrno(errEINVAL) // musl setenv: an invalid name is EINVAL (#657)
		return -1
	}
	defer EnvironSync() // refresh the C environ snapshot (#675, source/env.c)
	envMu.Lock()
	delete(envCache, n) // drop the stale cached copy; next getenv re-reads
	envMu.Unlock()
	return 0
}

//go:linkname Unsetenv
func Unsetenv(name *byte) int32 {
	n := cstr(name)
	if os.Unsetenv(n) != nil {
		return -1
	}
	defer EnvironSync() // refresh the C environ snapshot (#675)
	envMu.Lock()
	delete(envCache, n)
	envMu.Unlock()
	return 0
}

// environ (#675, source/env.c): the C `char **environ` is a rebuilt snapshot
// of the os environment. The snapshot bridge below builds a NULL-terminated
// char** whose array and strings are all libc-malloc'd (unmanaged C heap);
// env.c's __environ_sync (Go name EnvironSync) frees the previous one.
//go:linkname __c2go_environ_snapshot
func __c2go_environ_snapshot() **byte {
	env := os.Environ()
	arr := Malloc(uint64((len(env) + 1) * 8))
	if arr == nil {
		return nil
	}
	slots := unsafe.Slice((**byte)(arr), len(env)+1)
	for i, kv := range env {
		p := Malloc(uint64(len(kv) + 1))
		if p == nil {
			return nil // partial array is still consistent (NULL-terminated below)
		}
		b := unsafe.Slice((*byte)(p), len(kv)+1)
		copy(b, kv)
		b[len(kv)] = 0
		slots[i] = &b[0]
	}
	slots[len(env)] = nil
	return (**byte)(arr)
}

//go:linkname __c2go_os_clearenv
func __c2go_os_clearenv() {
	os.Clearenv()
	envMu.Lock()
	clear(envCache)
	envMu.Unlock()
}

// sched_yield (#675, <sched.h>): cooperative reschedule is the only
// meaningful yield under the Go scheduler; always succeeds.
//go:linkname sched_yield github.com/c2gohq/c2go_libc.sched_yield
func sched_yield() int32 { runtime.Gosched(); return 0 }

func init() {
	// Populate the C environ snapshot before any C code can read it.
	EnvironSync()
}
