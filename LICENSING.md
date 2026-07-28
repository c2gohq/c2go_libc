# Licensing model (pre-release)

[简体中文](LICENSING.zh-CN.md)

> **Status: PRE-RELEASE — NOT RELEASE-READY.**
>
> This document describes the intended licensing model and the work still
> required before a public release. It is not legal advice and does not itself
> grant a commercial license.

## Intended model

`c2go-libc` is a multi-license work rather than a uniformly authored codebase.
The intended release model is:

- Original c2go material: **GNU AGPL-3.0-only**, or a separately executed
  commercial agreement as an alternative.
- musl-derived material: musl's **MIT** terms and any file-specific permissive
  notices bundled with musl.
- Other third-party-derived material: its original license terms.
- Original c2go modifications in mixed files: AGPL-3.0-only or a separate
  commercial agreement, without changing the rights in the underlying
  third-party material.

The standard AGPL version 3 text is in [LICENSE](LICENSE). The exact musl
`COPYRIGHT` file currently used by the project is in
[LICENSES/musl-COPYRIGHT.txt](LICENSES/musl-COPYRIGHT.txt).

The project intentionally chooses `AGPL-3.0-only`, not an unspecified
`AGPL-3.0` and not `AGPL-3.0-or-later`. File-level notices have not yet been
completed, so this choice is not yet expressed consistently throughout the
tree.

## What the AGPL option means

The AGPL option permits commercial use. It does not mean that every commercial
user must pay. A user may use original c2go material under the AGPL if the user
can and does comply with the license as it applies to that use.

The planned commercial option is for users who need different terms for the
original c2go material. It cannot relicense, withdraw, or restrict rights in
musl or any other third-party component. See
[COMMERCIAL-LICENSING.md](COMMERCIAL-LICENSING.md).

No public runtime/library exception is currently intended for the AGPL path.
Adding one could materially change the commercial boundary and must not be done
without an explicit business decision and legal review.

## License boundaries

Licenses do **not** map one-to-one to directories.

| Area | Current facts | Intended treatment | Status |
| --- | --- | --- | --- |
| `musl/` | Modified musl fork pinned as a submodule at `a31facd31f63...` | Original musl and bundled third-party terms | Remote and gitlink established; file-specific notice inventory incomplete |
| `csrc/*.c` | Mix of original wrappers, rewrites, and musl/other adaptations | Per-file and, where necessary, per-portion treatment | Incomplete provenance audit |
| `csrc/include/` | Annotated interface headers with musl, Darwin/XNU, MinGW, and original material | Preserve applicable upstream rights; license original additions separately | Incomplete provenance audit |
| Hand-written root `*.go`/`*.s` | Primarily c2go runtime bridges; ownership headers largely absent | AGPL-3.0-only or commercial, subject to audit | Copyright owner and headers unresolved |
| Generated `libc_*.go/.s` | Tracked artifacts generated from mixed source inputs | Carry a mixed-source notice and refer to all applicable notices | Tracked; notice and reproducibility work incomplete |
| `dl/` | Required by current `c2go-bind`, but absent here | Must be migrated or eliminated; purego-derived/dependent portions require Apache review | Release blocker |
| Tests/build documents | Mostly project material, with some tests/data derived from prior or third-party work | Audit before assigning a single license | Incomplete |

Directory placement, a `Code generated` marker, or a c2go copyright notice does
not erase third-party rights.

## Generated files

The target-specific `.go` and Plan 9 `.s` files combine code produced from many
source files. They must not carry a header claiming that the whole artifact is
owned by c2go or governed only by the AGPL.

A future generated-artifact header should state, in substance:

```text
Generated artifact. Do not edit.

This file contains code generated from multiple source components. Original
c2go portions are available under AGPL-3.0-only or a separately executed
commercial agreement. Third-party portions remain under their original
licenses. See NOTICE and THIRD_PARTY_NOTICES.md.
```

The generator must emit that notice consistently, and the release artifact
must carry the referenced files.

## Corresponding source and reproducibility

The Go module layout intentionally excludes the nested `csrc` module and the
musl submodule from a normal proxy zip. The generated Plan 9 assembly is not a
substitute for the preferred source used to modify it.

Every public release therefore needs a separately verified, recursively
complete source bundle containing or unambiguously pinning:

1. the matching `c2go-clang` source and build identity;
2. the matching `c2go-bind` source and build identity;
3. this repository at the release tag;
4. the exact musl fork commit;
5. `csrc`, headers, generation scripts, and generated artifacts;
6. all applicable license and third-party notice files; and
7. instructions sufficient to reproduce the generated files.

The current checkout does not satisfy this release condition.

## Contributions and commercial relicensing

Commercial dual licensing requires the commercial licensor to hold sufficient
rights in accepted contributions. The repository currently has no finalized
CLA, copyright-owner identity, or contribution acceptance workflow.

Until those items are finalized, external code contributions must not be
merged. A DCO or `Signed-off-by` line alone does not automatically grant the
rights needed for commercial relicensing. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Release blockers

The licensing work is not complete until all of the following are resolved:

- identify the legal copyright owner and commercial licensor;
- add accurate file-level copyright and license notices;
- verify deterministic regeneration and accurate mixed-source notices for all
  tracked generated target artifacts;
- make clean recursive checkouts pass the complete supported matrix without
  local-only inputs;
- migrate or eliminate the missing `github.com/c2gohq/c2go_libc/dl` package;
- restore and verify the required notice for the Apple Libc/FreeBSD-derived
  `csrc/termios.c` code;
- determine the provenance and applicable treatment of XNU-derived Darwin ABI
  material and other MinGW/Darwin definitions;
- inventory the file-specific TRE, math, qsort, and other notices incorporated
  through the selected musl sources;
- publish complete corresponding source and reproducibility instructions;
- finalize a CLA and contribution policy; and
- obtain review from qualified open-source/IP counsel.

See [PROVENANCE.md](PROVENANCE.md) for evidence and the live audit table.
