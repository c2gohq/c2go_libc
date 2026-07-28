# Provenance and release-readiness record

> **Audit snapshot updated: 2026-07-28**
> **Status: PRE-RELEASE — NOT RELEASE-READY**

This record distinguishes verified repository facts from intended design and
unresolved provenance. It is not a substitute for legal review.

## Repository identity

| Item | Verified state at audit time |
| --- | --- |
| Local directory | `c2go-libc2` |
| Intended public project name | `c2go-libc` |
| Go module path | `github.com/c2gohq/c2go_libc` |
| Go language version in `go.mod` | `1.25.0` |
| c2go ABI epoch range | `1..1` |
| Repository branch/commit | `main`; code/submodule baseline `5ee9e47` before this documentation commit |
| Repository remote | None configured |
| README/CI | README prepared by this documentation change; CI absent |

The local suffix `2` is not part of the intended module identity. The
underscore in the Go module path is intentional and is embedded in linknames;
it must not be changed as a cosmetic repository rename.

## Build inputs

`CMakeLists.txt` is currently a source manifest parsed by `gen.sh`; it does not
define a runnable CMake target.

At audit time the manifest selected:

- 334 translation units from `musl/`; and
- all 28 C translation units under `csrc/`.

`csrc/include/` contains 59 headers used in translation. The generated runtime
also depends on hand-written Go bridges and two hand-written architecture
assembly files for setjmp/longjmp.

The generation pipeline is:

```text
selected C sources
  -> c2go-clang -fc2go (LLVM bitcode per translation unit)
  -> c2go-lto (whole-package Plan 9 assembly + manifest)
  -> c2go-bind (target-specific Go bindings)
  -> root Go module
```

The current target manifest is:

| GOOS | GOARCH | Generated runtime | Generated C selftest |
| --- | --- | --- | --- |
| `darwin` | `arm64` | Yes | Yes |
| `darwin` | `amd64` | Yes | Yes |
| `linux` | `arm64` | Yes | Yes |
| `linux` | `amd64` | Yes | Yes |
| `windows` | `amd64` | Yes, linked against `msvcrt` where requested | No |

This table describes generation targets, not a release-quality support claim.
There is no checked-in CI matrix proving them from a clean source checkout.

## Source-category audit

| Path/category | Verified provenance facts | Ownership/license conclusion | Audit status |
| --- | --- | --- | --- |
| `musl/` | Git submodule of `https://github.com/c2gohq/musl.git`; branch `c2go`; pinned commit `a31facd31f63...`; based on `b306b16...` | musl and bundled original terms remain applicable | Submodule/remote resolved; file-specific notice inventory remains incomplete |
| Selected musl TUs | 334 selected; pinned fork differs from base in 375 files overall (`+1226/-600`) | Cannot call the fork byte-verbatim or annotation-only | File-specific notice inventory incomplete |
| `csrc/stdio.c` | Header and body identify many musl stdio/math/stdlib implementations, including verbatim sections | Mixed musl-derived and original adaptations | Required per-file notice absent |
| `csrc/multibyte.c` | Explicitly identifies musl derivation and carries a musl copyright reference | Mixed musl-derived and original adaptations | Best current header, still needs final SPDX/legal review |
| `csrc/locale.c`, `intl.c`, `random.c`, `strsignal.c` | Comments identify musl algorithms, shapes, rewrites, or verbatim sections | Mixed provenance | Required per-file notices incomplete |
| `csrc/fsops_*`, `stat2.c`, `termios.c`, other POSIX wrappers | Combine independently written bridges with upstream shapes or implementations | Must be assessed file by file | Incomplete |
| `csrc/termios.c` Darwin branch | Identifies Apple Libc/FreeBSD source; local reference contains a BSD notice missing here | Third-party notice must be restored; original changes may be separately licensed | Release blocker |
| Darwin headers | Identify XNU structures/constants/command encodings | License significance of copied expression vs ABI facts unresolved | Release blocker |
| Windows headers/sources | Identify MinGW/MSVCRT values and interfaces | Source-copying history unresolved | Review required |
| Root hand-written Go | c2go runtime bridges; migrated from the prior local tree | Only one tracked non-test Go file currently has a copyright marker | Ownership headers and historical audit incomplete |
| Generated `libc_*.go/.s` | Five target pairs produced from all selected C inputs and tracked in Git | Mixed-source artifacts | Tracked; complete generated notice and reproducibility proof absent |
| `selftest` generated files | Four target pairs produced by the same pipeline and tracked in Git | Mixed-source artifacts and project tests | Tracked; complete generated notice and reproducibility proof absent |
| `dl/` | Migrated external-call bridge; Unix uses PureGo v0.11.0-alpha.8 private dispatcher/trampoline symbols, while Windows uses `x/sys/windows` loader APIs and `syscall.SyscallN` | c2go-specific code remains subject to ownership audit; PureGo remains Apache-2.0 | Exact dependency and license carrier recorded; clean-consumer gate remains |

The `csrc` directory is not a reliable license boundary. The old description
"our independent C (no musl basis)" is contradicted by current file contents
and must not appear in release licensing claims.

## License-header coverage snapshot

At the updated audit snapshot:

- 69 tracked non-test root Go files were present; 1 contained a copyright or SPDX
  marker found by the audit search.
- 87 tracked `csrc` C/header files were present; 1 contained an explicit
  copyright or SPDX marker found by the audit search.

Comments such as "ported from musl" are useful provenance clues but do not
replace required copyright and license notices.

## Reproducibility findings

The generated `libc_<os>_<arch>.go/.s` and four Darwin/Linux selftest artifact
pairs are now tracked. On Darwin arm64 with Go 1.25.9, `go test ./...` passed
both in the active checkout and in a clean `git archive HEAD` extraction.

The archive contains the generated files and a `musl/` gitlink directory, but
not the musl submodule contents. This proves that the checked-in artifacts can
build on the tested host; it does not prove deterministic regeneration,
cross-target correctness, or recursively complete corresponding source. A
separate clean-clone check successfully initialized the public musl submodule
at `a31facd31f63ac569a39f8796b7e5c1494892f1e`.

In the updated Darwin arm64 checkout, `go vet ./...` exited nonzero. Most
diagnostics came from the stock analyzer interpreting C2Go-generated Plan 9
assembly frames and symbols as ordinary Go assembly; it also reported a
possible `unsafe.Pointer` misuse in `fnmatch_glob_test.go`. These findings need
an explicit reviewed disposition and a reproducible static-analysis gate.

Therefore the passing build tests close the earlier missing-artifact failure,
but they are not evidence that the current Git tree is releasable.

## Version constraints

Generated bindings currently use build constraints equivalent to:

```text
go1.22 && !go1.26
```

Combined with `go 1.25.0` in `go.mod`, the effective supported Go line is
currently Go 1.25.x. A release must generate explicit failure stubs or provide
clear diagnostics outside the supported range and must test the exact Go patch
version used to build artifacts.

## Required release evidence

A release candidate is not ready until it provides all of the following:

1. A reachable public repository and immutable release tag.
2. A reachable c2go musl fork and committed submodule pointer.
3. Tracked or release-packaged generated artifacts for every claimed target.
4. A clean-checkout generation and test log.
5. End-to-end consumer tests with the matching `c2go-clang` and `c2go-bind`.
6. Clean-consumer verification of the migrated `dl` package and its pinned
   PureGo private-ABI boundary.
7. A complete file-level provenance table and third-party notice bundle.
8. Correct generated-file notices.
9. A documented, passing or explicitly reviewed vet/static-analysis gate.
10. A recursively complete corresponding-source archive.
11. Approved AGPL, commercial-license, CLA, and copyright-owner texts.
12. Qualified legal review of the mixed-license release.

Until every required item is closed, releases, module tags, binary bundles, and
marketing statements should continue to say **PRE-RELEASE / NOT READY FOR
PRODUCTION USE**.

As of 2026-07-28, item 2 and the artifact-tracking portion of item 3 are
complete. Their notice, reproducibility, release-tag, and full-matrix aspects
remain open.
