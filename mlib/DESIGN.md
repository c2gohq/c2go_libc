<!-- SPDX-License-Identifier: AGPL-3.0-only -->

# Managed libc architecture and migration inventory

This document records the boundary between the root unmanaged libc ABI and the
managed `mlib` ABI. It is an implementation inventory, not a promise that every
listed family is already available from `mlib`.

## Invariants

- Root libc keeps its existing source and ABI behavior. An unmanaged C record
  cannot be relied on to keep a Go pointer alive, so it stores an integer handle
  or uses another explicit Go root.
- An mlib state carrier stores a direct Go pointer and is described to C2Go as a
  managed record. Stack and global carriers are emitted with GC metadata; heap
  carriers must be allocated with typed `gc_malloc`.
- `mlib` never exposes or uses ordinary `malloc`, `realloc`, or `free`.
- Stateless implementations are shared. Go-owned state algorithms are shared
  below both carrier layers. C code is instantiated twice only when allocation,
  record layout, pointer copying, or returned object graphs differ.
- `C2GO_MLIB_UNPREFIXED` changes the public names only. It must not change these
  ownership rules or silently route a root carrier into an mlib function.

## Rooting-table inventory

`handleTable[T]` is the generation-stamped `registry + generations + freelist +
RWMutex` implementation in `handle.go`.

| Root table | Unmanaged carrier / lifetime | Root libc function family | Propagating consumers | mlib status |
| --- | --- | --- | --- | --- |
| `semTab` | `sem_t._id` | `sem_init`, `sem_destroy`, `sem_wait`, `sem_trywait`, `sem_timedwait`, `sem_post`, `sem_getvalue` | none outside the semaphore family | Implemented: direct pointer carrier over shared `internal/posixsync` state |
| `mutexTab` | `pthread_mutex_t._id` | mutex init/destroy/lock/try/timed/unlock and mutex attrs | condition waits share the same mutex state | Implemented |
| `condTab` | `pthread_cond_t._id` | cond init/destroy/wait/timedwait/signal/broadcast and attrs | mutex state during wait/reacquire | Implemented |
| `rwlockTab` | `pthread_rwlock_t._id` | rwlock init/destroy/read/write/try/unlock and attrs | none outside the rwlock family | Implemented |
| `threadTab` | scalar `pthread_t` ID | create/join/exit/detach/self/equal/yield | pthread-key destructor lifetime and goroutine-local state | Not replaced; unprefixed mlib pthread intentionally retains the root thread API |
| `dirTab` | `DIR.handle` | opendir/fdopendir/readdir/readdir_r/rewinddir/closedir/dirfd | `scandir`; directory traversal in `glob`; `nftw`/`ftw` | Complete: lifecycle plus managed `scandir`, `glob`, and Unix `nftw`/`ftw` |
| `fileLockTab` | `FILE.lockid` | internal file lock/trylock/unlock/drop | effectively every stdio operation using `FILE *` | Not implemented; part of the FILE cluster |
| `popenTab` | `FILE.pipe_id` | popen/pclose bridge | FILE close/pipe ownership and process wait | Not implementable independently of the FILE cluster |
| `iconvTab` | roots the real `*iconvState` returned as `iconv_t`; the state also records its table ID for close | iconv_open/iconv/iconv_close | none | Not implemented; a managed descriptor can remove the extra root, but `(iconv_t)-1` must remain only in the C wrapper |

The following roots have a similar motivation but are not interchangeable with
the state-carrier table and must not be migrated mechanically:

- `mallocRegistry` and `mallocFreeIdx` root raw unmanaged allocations. mlib
  deliberately does not wrap this allocator.
- `keyRoots` roots real `*pthreadKey` values because root `pthread_key_t` lives in
  unmanaged C memory. It belongs to a future thread/key design, not the sync
  carrier implemented now.
- process-wide caches such as timezone strings and environment strings preserve
  stable C views; they are not per-object mlib carriers.

## Current implementation shape

```text
root sem/pthread sync carrier (_id) ---+
                                        +--> internal/posixsync --> Go state
mlib carrier (direct managed pointer) --+

root DIR carrier (handle id) -----------+
                                        +--> internal/posixdir --> Go stream
mlib DIR (direct pointer, typed heap) ---+
```

No musl source is dual-compiled for semaphore or pthread synchronization. Their
behavior already lives in Go; only the carrier resolution differs. In
unprefixed pthread mode, `<c2go/mlib/pthread.h>` replaces mutex, condition
variable, and rwlock types/functions while retaining root `pthread_create`,
thread keys, `pthread_once`, and `pthread_atfork`.

## Stateful clusters still requiring selective C instantiation

### DIR cluster

The basic managed `DIR` lifecycle is implemented. `mlib_DIR` holds the direct
directory state pointer and is created by typed `gc_malloc`; `closedir` clears
the pointer and never calls ordinary `free`. The state/enumeration behavior is
shared with root libc through `internal/posixdir`, while the managed C carrier
is selectively instantiated once as `mlib_*`. Unprefixed headers route standard
names to that one instance rather than compiling a second copy.

`scandir` is now selectively instantiated for mlib. Its growable result uses a
one-pointer record as the repeated `gc_malloc_array` element descriptor; every
directory entry is a separate no-pointer Go-heap object. Growth copies elements
with typed pointer assignments, and an in-place heapsort swaps pointers through
write barriers instead of using bytewise `qsort`. The returned graph is GC-owned
and must never be passed to `free`.

Unix `nftw`/`ftw` are also selectively instantiated from the pinned musl
sources. The source-renaming wrapper substitutes mlib's managed `DIR`
operations while retaining the stateless traversal algorithm. Its recursive
history records remain on the managed C stack, callback arguments are borrowed,
and no heap container is introduced.

Managed `glob` completes the propagation cluster. Its `glob_t` carrier points
to a typed array of managed string pointers. Match-list nodes are fixed-size
typed objects and their variable-length strings are separate no-pointer
objects. `GLOB_APPEND` allocates a replacement vector and copies old slots with
write barriers; sorting uses typed swaps; `globfree` clears the carrier root.
No ordinary `malloc`, `realloc`, `qsort`, or `free` participates in this graph.

### FILE cluster

`FILE` has internal links, buffers, callbacks, a per-file lock handle, open-file
list membership, and optional popen process state. Its layout and ownership are
used throughout stdio, so changing only `fopen` or only the lock field would
create two incompatible FILE worlds. FILE/stdio/popen therefore remains the
last and largest selective-instantiation cluster.

## Next safe migration order

1. Keep semaphore and pthread sync as the reference pattern and retain their C
   and GC-stress tests on all release targets.
2. Design pthread thread/key ownership separately; do not extend the current
   unprefixed sync switch implicitly.
3. Keep the completed DIR propagation cluster under GC-stress regression.
4. Design and migrate FILE/stdio/popen as one cluster.

At every step the default API remains `mlib_`-prefixed, the unprefixed form is a
whole-LTO-package choice, and root libc must never import or depend on `mlib`.
