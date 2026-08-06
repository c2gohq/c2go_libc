# c2go-libc

[![Go Reference](https://pkg.go.dev/badge/github.com/c2gohq/c2go_libc.svg)](https://pkg.go.dev/github.com/c2gohq/c2go_libc)

[简体中文](README.zh-CN.md)

> **PRE-RELEASE — NOT RELEASE-READY**
>
> This repository is undergoing release, provenance, and licensing work. The
> project has not designated a stable module tag or production release. Anyone
> redistributing this snapshot must independently satisfy every applicable
> license and notice obligation. See [Release blockers](#release-blockers).

`c2go-libc` is the runtime C-library compatibility layer for programs
translated by the c2go toolchain. It combines selected musl-derived
implementations, c2go-specific C adaptations, and Go runtime bridges, then
generates target-specific Go bindings and Plan 9 assembly.

It is **not** a drop-in replacement for the host system libc. It is also not an
independent general-purpose Go library: its ABI, linknames, generated files,
and supported Go versions must match the corresponding `c2go-clang` and
`c2go-bind` release.

## Place in the c2go toolchain

```text
C source and annotated headers
        |
        v
c2go-clang -fc2go        -> LLVM bitcode per translation unit
        |
        v
c2go-lto                 -> whole-package Plan 9 assembly + manifest
        |
        v
c2go-bind                -> target-specific Go bindings
        |
        v
c2go-libc + translated package -> Go linker/runtime
```

The three c2go projects are a matched release set:

- **c2go-clang** owns the C/LLVM translation pipeline and c2go ABI lowering.
- **c2go-bind** turns the emitted manifest and assembly into a Go package.
- **c2go-libc** supplies the libc-compatible surface and Go runtime bridges
  expected by translated packages.

The Go module path is intentionally:

```text
github.com/c2gohq/c2go_libc
```

The underscore is embedded in active linknames and is different from the
human-facing repository name `c2go-libc`.

## Managed libc variants (`mlib`)

The root package keeps its unmanaged, handle-ID-based compatibility surface.
The new `mlib` subpackage is the managed counterpart for stateful APIs: its C
carrier stores a direct GC-visible Go pointer and therefore needs no global
ID-to-pointer registry. The implemented families cover unnamed semaphores;
pthread lifecycle, keys, and synchronization; the complete managed directory
propagation cluster (`DIR`, `scandir`, `nftw`/`ftw`, and `glob`); managed
standard, explicit, memory, custom, wide, and process streams; and managed
`search.h` tree, hash-table, and queue containers; and POSIX regular
expressions backed by a per-object GC arena. Managed allocation helpers include
`strdup`/`strndup`/`wcsdup`, `asprintf`/`vasprintf`, and `realpath`, with
GC-owned results.

Managed names are explicit by default (`mlib_sem_t`, `mlib_sem_init`,
`mlib_pthread_mutex_t`, `mlib_pthread_mutex_lock`, `mlib_DIR`,
`mlib_opendir`). Define
`C2GO_MLIB_UNPREFIXED` before the first mlib header to expose the corresponding
standard names instead. That switch applies to the whole C2Go/LTO package; do
not mix both routings in one package. Pointer-bearing managed records use typed
`gc_malloc(c2go_typeinfo(T), sizeof(T))`; pointer-free buffers may use
`gc_malloc(NULL, size)`, and type-erased object graphs need an explicit owner
such as the regex arena. They never use ordinary `malloc`. Managed
`scandir`, `glob`, and managed search containers are GC-owned and must not be
passed to `free`. The byte-oriented `lsearch`/`lfind` pair remains root-only
and may be used only with pointer-free elements. See
[mlib/README.md](mlib/README.md) for examples and constraints.

The current handle-table carrier migration is complete, but the managed
allocation surface is not. Remaining allocation-returning buffers and
pointer-bearing owned graphs are tracked separately. Managed POSIX regex is
implemented by selectively compiling a second TRE instance: all of its
allocations use `gc_malloc`, and a per-`regex_t` Go arena directly roots every
no-scan block instead of relying on pointers hidden inside TRE records.
`iconv` remains an intentional root-only exception:
exact POSIX compatibility requires the non-pointer `(iconv_t)-1` failure value,
which cannot be stored in a precise-GC managed pointer slot. mlib is therefore
a managed ownership surface, not a mechanical duplicate of every root-libc
function. Include the required `<c2go/mlib/...>` family headers explicitly;
there is deliberately no umbrella header for this partial surface.

## Current target manifest

The generator currently has entries for the following targets:

| GOOS | GOARCH | Runtime artifact | C selftest artifact |
| --- | --- | --- | --- |
| Darwin | arm64 | tracked generated artifact | tracked generated artifact |
| Darwin | amd64 | tracked generated artifact | tracked generated artifact |
| Linux | arm64 | tracked generated artifact | tracked generated artifact |
| Linux | amd64 | tracked generated artifact | tracked generated artifact |
| Windows | amd64 | tracked generated artifact; uses `msvcrt` for selected externals | not generated |

This table describes code-generation intent, **not a stable support promise**.
There is no checked-in clean-clone CI matrix yet. Windows support also has a
smaller POSIX surface than Darwin and Linux.

The current module declares Go 1.25.0. Its central `c2goabi` provider admits Go
1.25.x and Go 1.26.x under toolchain contract epoch 1, and rejects Go 1.27 or
later by default until that toolchain contract is validated. Schema-v2
generated packages do not embed that upper bound: if a later Go release keeps
contract epoch 1, only c2go-libc needs an update and those packages remain
unchanged. The current C2Go ABI epoch range is `1..1`.

## Repository layout

```text
.
├── go.mod                         Go module at the repository root
├── *.go                           hand-written Go runtime bridges
├── libc_<os>_<arch>.go/.s         generated release artifacts
├── sjlj_<arch>.s                  hand-written setjmp/longjmp support
├── musl/                          pinned c2go musl-fork submodule
├── csrc/                          C adaptations, original C, and headers
├── internal/posixsync/            state algorithms shared by libc and mlib
├── mlib/                          managed libc package, headers, and selftests
├── selftest/                      in-C callback/comparator tests
├── CMakeLists.txt                 source manifest consumed by gen.sh
├── gen.sh                         actual regeneration driver
└── LICENSE*, NOTICE, *LICENSING*  license and provenance records
```

Important details:

- `CMakeLists.txt` is currently a manifest. It does not define a runnable CMake
  build target.
- `musl/` is a committed submodule of `https://github.com/c2gohq/musl.git`,
  pinned to an exact commit from its `c2go` branch.
- `csrc/` is **not** a license boundary. It contains both original and adapted
  third-party-derived material.
- Generated runtime artifacts for all five listed targets and selftest
  artifacts for the four Darwin/Linux targets are tracked. Their deterministic
  regeneration and mixed-source notices still require release verification.
- `dl/` provides the external native-call boundary required by current
  `c2go-bind` output. Unix dispatch uses a version-pinned PureGo trampoline;
  Windows uses Win32 loader and syscall APIs.

See [PROVENANCE.md](PROVENANCE.md) for the evidence-backed source map.

## Local maintainer workflow

There is no supported end-user installation command yet. In particular,
`go get github.com/c2gohq/c2go_libc` is not a valid release instruction until
the blockers below are closed and an immutable version is published.

In the existing maintainer checkout, regeneration is driven by `gen.sh`:

```bash
CLANG=/absolute/path/to/c2go-clang \
C2GOLTO=/absolute/path/to/c2go-lto \
C2GOBIND=/absolute/path/to/c2go-bind \
./gen.sh

go test ./...
```

`CLANG`, `C2GOLTO`, and `C2GOBIND` must all come from a mutually compatible
c2go toolchain. The checked-in script still contains machine-specific default
paths; callers should override all three variables.

Initialize `musl/` with `git submodule update --init` before regenerating.
These commands are not evidence that the complete clean-clone matrix works.

## Testing status

At the 2026-07-28 verification snapshot:

- `go test ./...` passed on Darwin arm64 with Go 1.25.9 in the active checkout
  and in a clean `git archive HEAD` extraction using the tracked generated
  files.
- `go vet ./...` did not pass: the stock assembly analyzer reports the
  C2Go-generated nonstandard argument frames and missing Go declarations, and
  one test reports a possible `unsafe.Pointer` misuse. A release must define
  and enforce a reviewed vet/static-analysis policy rather than silently
  ignoring this result.
- A clean recursive clone can initialize the pinned musl submodule. A Git
  archive includes the gitlink directory but not the submodule contents, so
  the archive test proves use of tracked artifacts, not reproducible
  regeneration or complete corresponding source.
- No repository CI workflow currently proves all five generated targets.
- `dl` unit tests pass natively on Darwin arm64 with `CGO_ENABLED=0`; its
  Darwin amd64 and Linux amd64/arm64 packages cross-compile, and its
  Windows/amd64 loader plus integer/float dispatch tests pass under Wine 7.7.
  Generated consumers still need a clean-checkout end-to-end release gate.

At the 2026-08-03 contract-provider update, Go 1.26.0 passed the root, `dl`,
and `selftest` packages natively on Darwin arm64, including a SQLite consumer
under `GOGC=1` and `GODEBUG=invalidptr=1`. The coordinated toolchain release
workflow now requires the same Go 1.26.x runtime tests on native Linux amd64,
Linux arm64, Windows amd64, and macOS arm64 runners before packaging begins.

Release claims must be based on a clean checkout and end-to-end generated
consumer tests, not the prepared working tree alone.

## Scope and known limitations

The implementation intentionally exposes only the functions and declarations
that have an implementation for the relevant target. Current design limits
include:

- one `C.UTF-8` locale and no message catalogs;
- Go-runtime-mediated signal delivery rather than unrestricted native signal
  handler replacement;
- no general `fork`/`exec` family;
- a reduced POSIX surface on Windows;
- selected formatted-I/O paths treating `long double` as `double`;
- no 32-bit targets; and
- tight coupling to the supported Go version and c2go ABI epoch.

These limits should be treated as part of the current runtime contract, not
silently filled with fake-success stubs.

## Release blockers

The project will not designate this repository as **RELEASE-READY** until at
least the following items are complete:

1. Verify deterministic regeneration and add accurate mixed-source notices to
   every tracked generated artifact.
2. Make a clean recursive checkout build and pass the complete test matrix.
3. Remove machine-specific generator assumptions and document a reproducible
   toolchain bootstrap.
4. Verify `c2go_libc/dl` unmanaged-extern and callback consumers end to end
   from a clean matched toolchain checkout, including the pinned PureGo ABI.
5. Restore and verify the notice required by the Apple Libc/FreeBSD-derived
   Darwin code in `csrc/termios.c`.
6. Resolve the provenance and license treatment of XNU-derived Darwin ABI
   material and other third-party definitions.
7. Complete file-level copyright/license headers and a generated-artifact
   notice.
8. Resolve or explicitly review the current `go vet` findings and make the
   release static-analysis gate reproducible.
9. Publish a complete corresponding-source bundle containing the exact source
   used to produce shipped artifacts.
10. Record and verify the full legal identity and ownership of the selected
    individual licensor, finalize commercial terms and a CLA, and obtain
    qualified open-source/IP legal review.

The detailed checklist lives in [PROVENANCE.md](PROVENANCE.md) and
[LICENSING.md](LICENSING.md).

## Licensing

The intended model is:

- verified original c2go material: **GNU AGPL-3.0-only**, or a separately
  executed commercial agreement as an alternative;
- musl-derived material: musl's MIT terms and its file-specific notices; and
- all other third-party material: its original applicable terms.

AGPL-3.0-only permits commercial use when its conditions are followed. The
repository must not claim that all commercial use requires payment. A future
commercial agreement can grant alternative rights only for material that its
licensor is authorized to license; it cannot change rights in musl or other
third-party material.

GitHub Sponsors may be designated as a payment channel in a separately
executed commercial agreement. Sponsorship payment alone does not grant rights.

The current license/provenance documentation is deliberately marked
pre-release because the file-level audit is incomplete. Anyone independently
redistributing this snapshot must comply with all applicable licenses and
cannot rely on the current documents as a final notice bundle.

Read:

- [LICENSE](LICENSE) — GNU AGPL version 3 text;
- [NOTICE](NOTICE) — current multi-license pre-release notice;
- [LICENSING.md](LICENSING.md) — intended model and release conditions;
- [COMMERCIAL-LICENSING.md](COMMERCIAL-LICENSING.md) — commercial intent and
  current non-offer status;
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) — incomplete third-party
  inventory; and
- [PROVENANCE.md](PROVENANCE.md) — audit evidence and unresolved boundaries.

This documentation is not legal advice. The final release requires qualified
legal review.

## Contributing

External code contributions are not currently accepted for merge because the
copyright owner, CLA, and commercial-relicensing process are not finalized.
Reproducible bug reports and original documentation feedback may be provided
through whatever public issue channel is established for the eventual
repository.

See [CONTRIBUTING.md](CONTRIBUTING.md) before submitting any material.
