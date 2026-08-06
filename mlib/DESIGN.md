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
- GC metadata and pointer-store provenance are separate requirements. A direct
  Go-heap pointer stored in a carrier or typed container uses an explicit
  `managed` pointer type so it remains AS1 through LLVM and every heap/root
  update receives a Go write barrier. `mlib/gen.sh` rejects generated assembly
  if required carrier, pointer-container, or retirement barriers disappear.
- `mlib` never exposes or uses ordinary `malloc`, `realloc`, or `free`.
- Logical release is still explicit even though storage is GC-owned. Every
  managed `free`/`destroy`/`close`/`delete` equivalent clears carrier fields,
  container slots, and propagated roots before returning. If a POSIX API takes
  a pointer descriptor by value and therefore cannot rewrite the caller's
  variable, it clears all library-owned roots and the caller must discard or
  preferably assign `NULL` to that descriptor.
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
| `threadTab` | scalar `pthread_t` ID | create/join/exit/detach/self/equal/yield | pthread-key destructor lifetime and goroutine-local state | Implemented: mlib publishes the direct thread-state pointer; a table ID is allocated only if that thread deliberately crosses into root `pthread_self` |
| `dirTab` | `DIR.handle` | opendir/fdopendir/readdir/readdir_r/rewinddir/closedir/dirfd | `scandir`; directory traversal in `glob`; `nftw`/`ftw` | Complete: lifecycle plus managed `scandir`, `glob`, and Unix `nftw`/`ftw` |
| `fileLockTab` | `FILE.lockid` | internal file lock/trylock/unlock/drop | effectively every stdio operation using `FILE *` | Implemented: direct lock pointer in the managed FILE carrier |
| `popenTab` | `FILE.pipe_id` | popen/pclose bridge | FILE close/pipe ownership and process wait | Implemented: direct process pointer in the managed FILE carrier |
| `iconvTab` | roots the real `*iconvState` returned as `iconv_t`; the state also records its table ID for close | iconv_open/iconv/iconv_close | none | Intentional root-only exception: an exact POSIX `iconv_t` must represent `(iconv_t)-1`, which cannot inhabit a precise-GC pointer slot |

The following roots have a similar motivation but are not interchangeable with
the state-carrier table and must not be migrated mechanically:

- `mallocRegistry` and `mallocFreeIdx` root raw unmanaged allocations. mlib
  deliberately does not wrap this type-erased allocator: managed callers use
  `gc_malloc`, with type information wherever the allocation stores pointers.
  This does not make allocation-owning APIs safe to
  present as mlib functions without an ownership design. POSIX regex is the
  first pointer-graph example: the managed TRE instance routes every allocator
  call through no-scan `gc_malloc`, while a per-`regex_t` Go arena directly
  roots every returned block. TRE's internal pointers remain unmanaged
  navigation edges and are never trusted as GC ownership edges. This avoids
  both the root malloc registry and a new process-global regex table.
- `keyRoots` roots real `*pthreadKey` values only for root libc, whose
  `pthread_key_t` lives in unmanaged C memory. The mlib key slot is GC-visible
  and publishes the descriptor directly, so it does not enter `keyRoots`.
- process-wide caches such as timezone strings and environment strings preserve
  stable C views; they are not per-object mlib carriers.

## Current implementation shape

```text
root sem/pthread sync carrier (_id) ---+
                                        +--> internal/posixsync --> Go state
mlib carrier (direct managed pointer) --+

root pthread thread/key (ID/root) ------+
                                        +--> shared lifecycle/TLS core
mlib thread/key (direct pointers) ------+

root DIR carrier (handle id) -----------+
                                        +--> internal/posixdir --> Go stream
mlib DIR (direct pointer, typed heap) ---+

root FILE lock/process (handle ids) -----+
                                        +--> shared Go lock/process core
mlib FILE (direct managed pointers) -----+

root search containers (noscan malloc) --+--> musl search algorithms
mlib search containers (typed GC heap) --+

root regex (malloc-backed TRE) -----------+--> selectively shared TRE source
mlib regex (per-object GC arena) --------+
```

No musl source is dual-compiled for semaphore or pthread state. Their behavior
already lives in Go; only the carrier publication/resolution differs. In
unprefixed pthread mode, `<c2go/mlib/pthread.h>` replaces lifecycle, thread-key,
mutex, condition-variable, and rwlock types/functions. The plain-data
`pthread_once` carrier and stateless `pthread_atfork` remain shared from the
root header.

The regex family is intentionally different: its TRE C sources are compiled
once for root libc and once, under private `__mlib_*` symbols, for mlib. This is
a selective second instance of one allocation-sensitive family, not a second
build of the complete musl library.

## Stateful clusters and selective C instantiation

### semaphore and pthread synchronization cluster

Managed semaphore, mutex, condition-variable, and rwlock carriers each contain
one direct pointer to the shared `internal/posixsync` state. Their destroy
functions atomically clear that field. A stack or global carrier needs no
physical deallocation; a carrier allocated with typed `gc_malloc` has a second
lifetime edge in the caller, which must also be assigned `NULL` after destroy.
The focused retirement fixture holds all four carrier pointers in one typed
GC-heap owner and keeps generated allocation, destroy-route, and write-barrier
gates for both namespace modes.

### pthread lifecycle and key cluster

Managed `pthread_t` is a direct pointer to the shared Go thread state rather
than an ID in `threadTab`. The start argument and result are explicitly managed:
the goroutine closure retains the argument until the C callback starts, the
thread state retains the result until join, and join publishes it through a Go
write barrier. Join, detach, exit, self/equal, attributes, TLS keys, and TLS
destructors reuse the root Go behavior beneath a small selectively-instantiated
C wrapper.

Managed `pthread_key_t` similarly points straight at the shared key descriptor.
The GC-visible key slot replaces root libc's `keyRoots` entry, while each
goroutine's TLS map keeps non-null managed values alive and runs the existing
bounded destructor loop on exit. `pthread_once_t` contains only integer state,
so it needs no alternate managed carrier and remains shared in unprefixed mode.
Do not pass root thread/key carriers to mlib or mlib carriers to root libc.

### DIR cluster

The basic managed `DIR` lifecycle is implemented. `mlib_DIR` holds the direct
directory state pointer and is created by typed `gc_malloc`; `closedir` clears
the pointer and never calls ordinary `free`. The state/enumeration behavior is
shared with root libc through `internal/posixdir`, while the managed C carrier
is selectively instantiated once as `mlib_*`. Unprefixed headers route standard
names to that one instance rather than compiling a second copy. Since POSIX
`closedir` receives `DIR *` by value, it cannot clear the caller's owner;
internal users and public callers must discard borrowed entries and assign the
closed `DIR *` slot `NULL`.

`scandir` is now selectively instantiated for mlib. Its growable result uses a
one-pointer record as the repeated `gc_malloc_array` element descriptor; every
directory entry is a separate no-pointer Go-heap object. Growth copies elements
with typed pointer assignments, and an in-place heapsort swaps pointers through
write barriers instead of using bytewise `qsort`. The returned graph is GC-owned
and must never be passed to `free`; the caller logically releases it by assigning
the last managed result owner `NULL`.

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
If a typed-GC-allocated `glob_t` carrier is itself retired, its caller-owned
pointer is a separate lifetime edge and must also be assigned `NULL`.

### Search-container cluster

The managed search header selectively instantiates musl's AVL tree, open-address
hash table, and linked-queue algorithms. Root libc keeps its ordinary
malloc-backed, noscan containers and its stable-unmanaged-pointer contract.
The mlib instance instead allocates every internal tree node and hash array
with typed `gc_malloc`; hash entries, tree keys, and queue links are explicit
managed fields. Rotations, rehash copies, insertion, unlinking, and retirement
therefore preserve pointer metadata and Go write barriers. The global and
reentrant hash APIs share this same managed implementation.

The public algorithm shape is shared, but the storage instance cannot be: a
root node is a noscan C object while an mlib node is a precisely scanned Go
object. `tdelete`, `tdestroy`, `hdestroy`, and `remque` clear managed roots and
let the GC reclaim unreachable storage rather than calling `free`. Since
`tdestroy` receives the tree root by value, its caller explicitly clears the
last root variable after destruction.

`lsearch` and `lfind` are deliberately not instantiated for mlib. Their
`void * + element width` interface performs byte copies without carrying a C
element type, so no implementation can derive the pointer bitmap needed for a
pointer-bearing managed element. Both namespace modes retain the root functions
for pointer-free elements only.

### Regex cluster

Managed `regcomp`, `regexec`, and `regfree` selectively instantiate the pinned
TRE sources under private mlib symbols. `regex_t` carries two managed roots:
the compiled TNFA entry and its owning Go arena. Every TRE `malloc`, `calloc`,
`realloc`, and `free` operation is redirected to that arena and ultimately to
no-scan `gc_malloc`; `mlib/gen.sh` rejects an artifact that still calls a root
allocator.

The no-scan choice is deliberate. TRE allocates heterogeneous records and
slabs through a type-erased allocator, so one correct repeated pointer bitmap
does not exist. Instead, the arena's Go map directly roots every allocation
base. Internal record pointers are used only for C navigation. A successful
compile publishes the arena through a write barrier. Each `regexec` uses an
independent goroutine-local temporary arena, so concurrent matches share only
the immutable compiled TNFA. `regfree` clears the carrier roots and lets the Go
GC reclaim the whole graph; callers must not pass any part of it to `free`. A
heap `regex_t` carrier has one additional caller-owned edge, which must be set
to `NULL` after `regfree` when that carrier is retired.

### FILE cluster

The first explicit-stream FILE surface is implemented. An `mlib_FILE` is a
typed Go-heap carrier with two deliberately separate regions:

- a fixed-size, aligned no-scan envelope containing root libc's raw musl FILE
  engine; and
- explicit managed roots for its GC buffer, direct recursive lock, managed
  open-file-list links, and optional memory/cookie/process ownership.

Root libc supplies internal raw init/close helpers and the stateless buffering,
formatting, read, write, seek, and status algorithms. Those helpers never
allocate the carrier, join root libc's open-file list, or touch `fileLockTab`.
The managed wrapper uses `gc_malloc`, its own managed list for `fflush(NULL)`,
and the shared `internal/posixstdio` lock algorithm. Thus the raw engine may
hold borrowed addresses into its buffer while the scanner sees only the
parallel managed owner fields.

Implemented now: `fopen`/`fdopen`/`fclose`, managed `stdin`/`stdout`/`stderr`,
flushing (including `NULL` and exit-time hooks), block and character I/O,
positioning/status, formatted output/input, managed `getline`/`getdelim`,
managed `fmemopen`, descriptor access, and the `flockfile` family. Narrow scanf
and line-reading parsers remain shared implementations, but their managed policy
allocates result buffers with `gc_malloc`, retains growing buffers in managed
locals, and publishes replacement pointers through a checked write-barrier
helper. `fmemopen` shares the raw memory-stream engine while its managed carrier
retains either the caller buffer or a GC-owned replacement. Managed
`open_memstream` has a separate cookie policy: it grows a no-pointer GC buffer,
retains the caller's two result-slot addresses in the carrier, and publishes
each replacement buffer through write barriers. Because those addresses escape
until close, they must refer to GC-visible long-lived storage, normally fields
of a typed `gc_malloc` record; C stack locals are invalid. Managed
`fopencookie` keeps its cookie as a direct managed carrier root, while its four
code pointers remain in the carrier's no-scan callback envelope. The callbacks
are synchronous internal-ABI functions: the cookie parameter is explicitly
managed, while data buffers and the seek-position pointer are borrowed and may
not escape. The root policy continues to use ordinary malloc/realloc/free and
caller-managed cookie lifetime. Wide-stream orientation, character/string I/O,
pushback, and formatted input/output reuse root libc's lock-free UTF, formatting,
and scanning engines beneath the managed carrier lock. Wide scanf selects the
same managed result policy as narrow scanf: `%m` buffers use `gc_malloc`, while
`%m` and `%p` pointers are published through the checked write-barrier helper.
Managed `popen`/`pclose` reuse root libc's platform-specific process launch,
descriptor handoff, and wait-status logic. Root libc still puts `*exec.Cmd` in
`popenTab`; mlib instead stores that direct pointer in a dedicated managed FILE
field. `pclose` first flushes and closes the raw pipe, then waits while a managed
local keeps the process alive. A process stream must be retired with `pclose`,
not plain `fclose`, so the child is reaped. No mlib FILE may be routed to a root
public FILE declaration.

`fclose` clears every managed carrier field after the raw close: object,
process, retained result-slot addresses, buffer, list links, and—after its final
unlock—the direct lock state. It also removes a matching `stdin`/`stdout`/
`stderr` package-global root, so the next lookup cannot return a closed carrier.
The POSIX by-value parameter cannot clear the caller's `FILE *`; callers and
fixtures must assign that final owner `NULL` after `fclose` or `pclose`.

### String-allocation cluster

Managed `strdup`, `strndup`, and `wcsdup` share root libc's stateless
length/copy functions but instantiate a separate ownership policy: each result
is a no-pointer `gc_malloc` buffer. `<c2go/mlib/string.h>` owns the narrow pair;
`<c2go/mlib/wstring.h>` owns only `wcsdup` and intentionally does not import the
managed FILE surface. The broader `<c2go/mlib/wchar.h>` includes that focused
header for users that also select managed wide-stream I/O.

In replacement mode the standard source spellings are macros targeting the
single prefixed implementation. This is necessary for `strdup`/`strndup`, which
LLVM treats as libcalls: giving the exact same IR function name both root and
managed link routes across different translation units is ambiguous. The
generated package therefore still contains one `mlib_` implementation. Results
must never reach ordinary `free`; the caller retires them by assigning the last
managed owner to `NULL`.

### Formatted-string allocation cluster

Managed `asprintf` and `vasprintf` share root libc's stateless `vsnprintf`
engine but allocate the returned no-pointer buffer with `gc_malloc`. Their
output slot is synchronous and is never retained: it may be a managed local in
the current frame or a field of a typed GC object. `mlib_vasprintf` first clears
the slot through a managed write barrier, keeps it `NULL` on measurement,
allocation, or formatting failure, then publishes only the complete result.
The caller logically releases that result by assigning the last managed owner
to `NULL`, never by calling ordinary `free`.

As with the string-duplication family, unprefixed source names are aliases to
one `mlib_` IR implementation. This keeps root and managed libcall routes from
becoming ambiguous when multiple translation units enter the same LTO package.

### Path-allocation cluster

Managed `realpath` shares root libc's `__c2go_realpath` Go bridge. A null output
selects a no-scan `gc_malloc(PATH_MAX)` result; bridge failure explicitly drops
the local owner before returning `NULL`. The standard signature conditionally
returns either a caller buffer or an allocation, which cannot be represented as
both unmanaged and precise managed address spaces. mlib therefore requires a
non-null caller buffer to be managed no-pointer storage as well. Root `realpath`
remains the correct API for stack or root-malloc buffers.

The focused `<c2go/mlib/stdlib.h>` includes the ordinary stdlib surface and
overrides only `realpath` in replacement mode. Both namespace modes route to one
`mlib_realpath` implementation. Tests hold allocated and caller buffers in an
explicit typed owner, exercise failed replacement, and clear every final root.

Managed `getcwd` similarly shares `__c2go_syscall_getcwd`. A null buffer uses a
no-scan `gc_malloc(PATH_MAX)` allocation and preserves root libc's rule that
`size` is ignored in allocation mode. A caller buffer remains borrowed and must
be managed storage so the return type stays precise. The focused
`<c2go/mlib/unistd.h>` overrides only `getcwd`; tests distinguish retirement of
an internally allocated/replaced owner from a failed borrow of caller storage.

## Managed allocation inventory

Handle-table carriers and allocation ownership are separate migration axes.
The former is closed for the currently implemented tables; the latter remains
open for APIs that create caller-owned storage or persistent pointer graphs.

| Root allocation surface | Managed treatment | Status |
| --- | --- | --- |
| `strdup`, `strndup`, `wcsdup` | allocate GC-owned no-pointer byte/rune storage; caller retires the last root by assigning `NULL` | Implemented |
| `asprintf`, `vasprintf` | clear the managed output slot, format into a GC-owned no-pointer buffer, then publish through a write barrier; caller retires the last owner with `NULL` | Implemented |
| `realpath(path, NULL)` | share the Go canonicalisation bridge, allocate with `gc_malloc`, and require any caller buffer to be managed so the conditional return stays precise | Implemented |
| `getcwd(NULL, size)` | share the syscall bridge, allocate with `gc_malloc`, and require any caller buffer to be managed so the conditional return stays precise | Implemented |
| `regcomp`, `regexec`, `regfree` | selectively instantiate TRE; every no-scan GC block is directly rooted by a per-object arena, and each match uses a temporary arena | Implemented |
| generic `malloc`/`calloc`/`realloc`/`free` | no mlib equivalent: their type-erased ABI cannot describe a precise GC bitmap | Intentionally root-only |

No pending family may be added to an mlib public header until its generated
assembly passes the no-root-allocator gate and its ownership/GC-stress tests.

## Closure and maintenance gates

The handle-table migration set is closed for the currently implemented libc
surface. Every root table with a managed-equivalent ABI has a direct-pointer
mlib carrier. This statement does not close the managed allocation inventory
above. Keep the following gates in place:

1. Run semaphore and the complete pthread state family under C, race, and
   GC-stress regression on all release targets; require destroy to clear every
   carrier state and typed heap owners to clear their final carrier pointers.
2. Run the DIR propagation cluster under GC-stress regression; verify
   `closedir` retires the carrier state and every internal/caller owner is set
   to `NULL`, including the final `scandir` result owner.
3. Run the managed FILE process-stream phase under native and GC-stress
   regression; verify close retires the direct lock and standard-stream global
   roots, then require caller-owned `FILE *` values to be set to `NULL`.
4. Run managed search trees, hash tables, and queues under GC-stress and
   generated write-barrier regression; keep `lsearch`/`lfind` pointer-free.
5. Run managed regex compile/match/free under forced GC in both namespace
   modes; keep its arena publication/retirement barrier gates enabled.
6. Run managed `strdup`/`strndup`/`wcsdup` through both namespace modes under
   forced GC; keep the final owner-to-`NULL` release pattern in the fixtures.
7. Run managed `asprintf`/`vasprintf` through both namespace modes under forced
   GC; retain the generated `GCMalloc`, output-slot clearing, and publication
   gates.
8. Run managed `realpath` through both namespace modes under forced GC; retain
   its `GCMalloc`, precise owner, replacement-failure, and final-`NULL` gates.
9. Run managed `getcwd` through both namespace modes under forced GC; preserve
   the distinction between retiring an internal/replaced owner and borrowing a
   caller buffer, plus the final-`NULL` gates.
10. Reject any generated mlib library that calls root `malloc`, `calloc`,
   `realloc`, or `free`; `mlib/gen.sh` enforces this on every target.

`iconvTab` is not unfinished migration work. An mlib descriptor that stores the
Go state directly would need either a nullable, non-POSIX failure convention or
a different public carrier/signature. It therefore could not participate in
`C2GO_MLIB_UNPREFIXED` as an exact replacement. Unless a separate nonstandard
API is deliberately designed, strict POSIX iconv stays in root libc and keeps
its explicit lifetime root.

At all times the default API remains `mlib_`-prefixed, the unprefixed form is a
whole-LTO-package choice, and root libc must never import or depend on `mlib`.
