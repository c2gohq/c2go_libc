# Third-party notices inventory (pre-release)

> **Status: PRE-RELEASE — INCOMPLETE. NOT RELEASE-READY.**

This file is an inventory, not the final third-party-notice bundle. It records
known sources and unresolved review work without reproducing license text that
has not yet been verified for the exact released files.

## musl libc

The build selects hundreds of translation units from a modified musl libc
fork, and several files outside the fork contain musl-derived adaptations.

- Upstream family: musl libc
- c2go fork: `https://github.com/c2gohq/musl.git`, branch `c2go`
- Pinned upstream fork base: `b306b16af15c89a04d8e0c55cac2dadbeb39c083`
- Pinned c2go fork commit:
  `a31facd31f63ac569a39f8796b7e5c1494892f1e`
- Primary terms: MIT, together with file-specific permissive notices described
  by musl
- Current license carrier:
  [LICENSES/musl-COPYRIGHT.txt](LICENSES/musl-COPYRIGHT.txt)

The current `LICENSES/musl-COPYRIGHT.txt` is byte-identical to the `COPYRIGHT`
file at the pinned submodule commit. That file explains that musl also contains
third-party works whose full notices live in individual source files.

Before release, the project must determine which of those files contribute to
the generated artifacts and reproduce every notice required for source and
binary redistribution. A Go module zip does not contain the musl submodule, so
a statement that a notice is "in the source file" is insufficient for an
artifact-only distribution.

Known musl-bundled categories selected by the build include at least:

- TRE regular-expression sources and their BSD-style notice;
- Sun/FreeBSD and other permissively licensed math sources whose notices are
  carried in individual files;
- the smoothsort implementation and its MIT-style notice; and
- the wider set described by musl's `COPYRIGHT` file.

Their final texts must be copied from the exact pinned source revision only
after the selected-file inventory is complete.

## Apple Libc / FreeBSD termios implementation

`csrc/termios.c` states that its Darwin branch follows Apple Libc's
FreeBSD-derived `gen/termios.c`. The locally identified reference contains a
University of California/Berkeley copyright and BSD redistribution terms, but
that header is absent from the current c2go file.

This is a release blocker. Before the project publishes a source or binary
release, it must:

1. identify the exact upstream revision and source path used;
2. restore the required source notice without modification;
3. include any required binary-distribution notice in the release bundle; and
4. record which c2go changes are original and separately licensable.

This pre-release inventory deliberately does not paste a guessed or
version-mismatched BSD text.

## Darwin/XNU ABI material

Several headers identify XNU as the source of Darwin structures, constants, or
command encodings, including:

- `csrc/include/termios.h`;
- `csrc/include/sys/ioctl.h`;
- `csrc/include/sys/resource.h`;
- portions of `csrc/include/bits/errno.h`, `bits/fcntl.h`, and
  `bits/signal.h`; and
- related Darwin bridge code.

Some entries may be uncopyrightable ABI facts; others may reflect copied or
adapted expression. The repository does not yet contain enough evidence to
assign a final license treatment. The exact source revision, copying history,
and any applicable Apple Public Source License or BSD notices require a
file-by-file review by qualified counsel. If necessary, the affected material
should be independently re-expressed from authoritative ABI specifications
with a documented clean provenance trail.

No APSL or BSD license text is asserted for these files by this inventory.

## MinGW and Microsoft runtime interfaces

Windows sources and headers use MinGW-compatible constants and declarations
and call `msvcrt`. Referencing a platform ABI or dynamically linking a system
library is not, by itself, proof that source code was copied. The repository
nevertheless needs a source-history audit for the Windows definitions before
claiming that all expression in those files is original.

No MinGW or Microsoft source license is asserted by this inventory.

## Go module dependencies

The current [go.mod](go.mod) declares:

| Module | Version | Locally observed license family | Release action |
| --- | --- | --- | --- |
| `github.com/timandy/routine` | `v1.1.6` | Apache-2.0 | Verify exact module and include notices required by the release form |
| `golang.org/x/sys` | `v0.47.0` | BSD-3-Clause-style Go license | Verify and include binary notices where applicable |
| `golang.org/x/text` | `v0.40.0` | BSD-3-Clause-style Go license | Verify and include binary notices where applicable |

These dependencies are not vendored in this repository. A final source or
binary distribution must apply the obligations appropriate to what it
actually includes.

## purego and the missing `dl` package

The current `c2go-bind` implementation can emit imports of
`github.com/c2gohq/c2go_libc/dl`, but this repository does not contain that
subpackage. The older local implementation depended on and mirrored internal
interfaces from `github.com/ebitengine/purego`, which uses Apache-2.0.

If `dl` is migrated, its copied, adapted, linked, and independently authored
portions must be audited and the exact purego version must replace any local
filesystem substitution. If the feature is redesigned away, the release must
prove that no generated package imports `dl`.

## Finalization rule

Before release, replace this inventory status with a generated or manually
verified list that maps every shipped third-party component to:

- exact project, revision, and source path;
- files or artifact regions that contain it;
- copyright holder(s);
- SPDX identifier where legally confirmed;
- full required notice text; and
- source and binary redistribution obligations.

Do not delete an upstream notice merely because code was reformatted,
translated, generated into assembly, or combined with original c2go code.
