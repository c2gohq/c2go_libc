#!/usr/bin/env bash
# gen.sh — regenerate c2go-libc2's Go bindings + Plan 9 .s via the c2go toolchain.
#
# Source list is read from CMakeLists.txt (the C2GO_MUSL_SOURCES + C2GO_OWN_SOURCES
# set() blocks). For musl sources, a same-named src/<domain>/<arch>/*.c replaces
# the baseline source exactly as it does in musl's own build. Public headers =
# csrc/include (our annotated, musl-derived headers).
# Every migrated musl TU is musl's original .c patched in place with `#include <c2go.h>`
# + a `c2go_extern` on each exported definition; the c2go_linkname lives in the header.
#
# UNIFIED (whole-package) build, same shape as the old c2go-libc gen.sh: each source
# compiles to LLVM bitcode; c2go-lto merges the whole package into one
# libc_<goos>_<arch>.s + manifest; c2go-bind emits one libc_<goos>_<arch>.go. Output
# lands at the module ROOT (module github.com/c2gohq/c2go_libc, `package libc`).
#
# clang / c2go-lto / c2go-bind are host binaries. bash 3.2 safe (macOS default).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MOD=github.com/c2gohq/c2go_libc
CLANG="${CLANG:-/Users/dexter/Downloads/llvm-project/build/bin/clang}"
C2GOLTO="${C2GOLTO:-/Users/dexter/Downloads/llvm-project/build/bin/c2go-lto}"
C2GOBIND="${C2GOBIND:-/Users/dexter/Downloads/c2go/c2go-bind/c2go-bind}"
RES="$("$CLANG" -print-resource-dir)/include"
MUSL_DIR="$ROOT/musl"; CSRC_DIR="$ROOT/csrc"; INC="$CSRC_DIR/include"

# Pull the .c paths out of CMakeLists.txt, expanding ${MUSL_DIR}/${CSRC_DIR}.
SOURCES=()
while IFS= read -r line; do
	[ -n "$line" ] && SOURCES+=("$line")
done < <(sed 's/#.*//' "$ROOT/CMakeLists.txt" \
	| grep -oE '\$\{(MUSL_DIR|CSRC_DIR)\}/[A-Za-z0-9_./+-]+\.c' \
	| sed -e "s#\${MUSL_DIR}#$MUSL_DIR#" -e "s#\${CSRC_DIR}#$CSRC_DIR#")
if [ "${#SOURCES[@]}" -eq 0 ]; then echo "gen.sh: no .c sources listed in CMakeLists.txt" >&2; exit 1; fi

# goos:arch:triple[:dynlib] — one self-contained libc_<goos>_<arch>.{go,s} per target.
LIBC_TARGETS=(
	"darwin:arm64:aarch64-apple-darwin"
	"darwin:amd64:x86_64-apple-darwin"
	"linux:arm64:aarch64-unknown-linux-goabi"
	"linux:amd64:x86_64-unknown-linux-goabi"
	"windows:amd64:x86_64-pc-windows-goabi:msvcrt"
)

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
echo "== libc ($MOD) : ${#SOURCES[@]} sources =="
for t in "${LIBC_TARGETS[@]}"; do
	IFS=: read -r goos arch triple lib <<<"$t"
	case "$arch" in
		arm64) musl_arch=aarch64 ;;
		amd64) musl_arch=x86_64 ;;
		*) echo "gen.sh: no musl architecture mapping for $arch" >&2; exit 1 ;;
	esac
	target_sources=()
	arch_overrides=0
	for src in "${SOURCES[@]}"; do
		case "$src" in
			"$MUSL_DIR"/*)
				rel="${src#"$MUSL_DIR"/}"
				arch_src="$MUSL_DIR/${rel%/*}/$musl_arch/${rel##*/}"
				if [ -f "$arch_src" ]; then
					src="$arch_src"
					arch_overrides=$((arch_overrides + 1))
				fi
				;;
		esac
		target_sources+=("$src")
	done
	bcs=()
	for src in "${target_sources[@]}"; do
		b="$(printf '%s' "$src" | sed 's#[/.]#_#g')"
		# -fno-math-errno: we DEFINE the math functions, so __builtin_sqrt & friends
		# lower to the hardware op, never a self-recursive call. See old gen.sh.
		"$CLANG" --target="$triple" -fc2go -fc2go-package="$MOD" -fno-math-errno \
			-emit-llvm -I "$RES" -I "$INC" \
			-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
		bcs+=("$tmp/${b}.${goos}_${arch}.bc")
	done
	asm="$tmp/libc_${goos}_${arch}.s"; json="$tmp/libc_${goos}_${arch}.json"
	# --c2go-escape-nonfatal: stack->heap escapes are diagnostics, not failures (musl
	# stdio swaps transient stack buffers under lock; matches the -fc2go driver).
	"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
		--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
	cp "$asm" "$ROOT/libc_${goos}_${arch}.s"
	out="$tmp/out_${goos}_${arch}"; mkdir -p "$out"
	"$C2GOBIND" -pkg="$MOD" -pkgname=libc -goname="libc_${goos}_${arch}" \
		-sidecar="$json" -out="$out" ${lib:+-l "$lib"} "$asm"
	cp "$out/libc_${goos}_${arch}.go" "$ROOT/libc_${goos}_${arch}.go"
	echo "  -> libc_${goos}_${arch}.{go,s} ($arch_overrides musl arch overrides)"
done

# selftest: in-C exercises of libc functions whose comparator/callback is
# invoked FROM C (qsort/bsearch/tsearch/...), so a Go closure cannot drive them.
# One package (selftest/) built from selftest/source/*.c; the Go tests there
# force-link the root libc package for the cross-package linkname symbols.
# HOST targets only (windows has no selftest port; its e2e runs under wine gates).
SELFTEST_TARGETS=(
	"darwin:arm64:aarch64-apple-darwin"
	"darwin:amd64:x86_64-apple-darwin"
	"linux:arm64:aarch64-unknown-linux-goabi"
	"linux:amd64:x86_64-unknown-linux-goabi"
)
echo "== selftest ($MOD/selftest) : $(ls "$ROOT"/selftest/source/*.c | wc -l | tr -d ' ') sources =="
for t in "${SELFTEST_TARGETS[@]}"; do
	IFS=: read -r goos arch triple lib <<<"$t"
	bcs=()
	for src in "$ROOT"/selftest/source/*.c; do
		b="$(printf '%s' "$src" | sed 's#[/.]#_#g')"
		"$CLANG" --target="$triple" -fc2go -fc2go-package="$MOD/selftest" -fno-math-errno \
			-emit-llvm -I "$RES" -I "$INC" \
			-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
		bcs+=("$tmp/${b}.${goos}_${arch}.bc")
	done
	asm="$tmp/selftest_${goos}_${arch}.s"; json="$tmp/selftest_${goos}_${arch}.json"
	"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
		--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
	cp "$asm" "$ROOT/selftest/selftest_${goos}_${arch}.s"
	out="$tmp/out_selftest_${goos}_${arch}"; mkdir -p "$out"
	"$C2GOBIND" -pkg="$MOD/selftest" -pkgname=selftest -goname="selftest_${goos}_${arch}" \
		-sidecar="$json" -out="$out" "$asm"
	cp "$out/selftest_${goos}_${arch}.go" "$ROOT/selftest/selftest_${goos}_${arch}.go"
	echo "  -> selftest/selftest_${goos}_${arch}.{go,s}"
done
echo "gen.sh: done."
