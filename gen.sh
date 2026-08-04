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
GOFMT="${GOFMT:-gofmt}"
RES="$("$CLANG" -print-resource-dir)/include"
MUSL_DIR="$ROOT/musl"; CSRC_DIR="$ROOT/csrc"; INC="$CSRC_DIR/include"

normalize_asm_eof() {
	awk '
		NF {
			while (blank > 0) { print ""; blank-- }
			print
			next
		}
		{ blank++ }
	' "$1" > "$1.tmp"
	mv "$1.tmp" "$1"
}

# Go's amd64 stack is only 8-byte aligned. The Plan 9 printer rewrites aligned
# 128-bit stack moves to their unaligned equivalents; keep generation
# fail-closed if a new opcode family ever bypasses that rewrite.
verify_amd64_stack_moves() {
	local asm_path="$1"
	local unsafe_pattern='^[[:space:]]*(V?MOVAPS|V?MOVAPD|MOVO|VMOVDQA(32|64)?)[[:space:]].*\((BP|SP)\)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "gen.sh: unsafe aligned amd64 stack move in $asm_path" >&2
		grep -nE "$unsafe_pattern" "$asm_path" >&2
		exit 1
	fi
}

# Go's arm64 assembler preserves LR for non-leaf functions because CALL marks
# them as non-leaf. An LLVM leaf must therefore never allocate R30 as scratch:
# RET would branch to the scratch value instead of returning to its caller.
verify_arm64_link_register() {
	local asm_path="$1"
	local unsafe_pattern='(^|[^[:alnum:]_])R30([^[:alnum:]_]|$)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "gen.sh: allocated arm64 link register in $asm_path" >&2
		grep -nE "$unsafe_pattern" "$asm_path" >&2
		exit 1
	fi
}

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
	# Keep the release build optimized. Go's amd64 stack is only 8-byte aligned;
	# LLVM's SLP vectorizer may raise ordinary local slots to 16-byte alignment,
	# which c2go-lto correctly rejects because the Go frame cannot guarantee it.
	opt_flags=(-O2)
	[ "$arch" = amd64 ] && opt_flags+=(-fno-slp-vectorize)
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
		# These bitcodes are linked manually below, so request the same pre-link
		# phase boundary that the clang driver's automatic c2go-lto route uses.
		"$CLANG" --target="$triple" -fc2go -fc2go-package="$MOD" -Xclang -fc2go-lto-prelink \
			"${opt_flags[@]}" -fno-math-errno \
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
	normalize_asm_eof "$ROOT/libc_${goos}_${arch}.s"
	case "$arch" in
		amd64) verify_amd64_stack_moves "$ROOT/libc_${goos}_${arch}.s" ;;
		arm64) verify_arm64_link_register "$ROOT/libc_${goos}_${arch}.s" ;;
	esac
	out="$tmp/out_${goos}_${arch}"; mkdir -p "$out"
	"$C2GOBIND" -pkgname=libc -goname="libc_${goos}_${arch}" \
		-sidecar="$json" -out="$out" ${lib:+-l "$lib"} "$asm"
	cp "$out/libc_${goos}_${arch}.go" "$ROOT/libc_${goos}_${arch}.go"
	# Schema-v2 anchors are OS-neutral and byte-identical across the target
	# matrix. Copy on every iteration so a missing/changed anchor fails this
	# release generation instead of silently shipping unguarded assembly.
	cp "$out/c2go_abi_anchor.go" "$ROOT/c2go_abi_anchor.go"
	"$GOFMT" -w "$ROOT/libc_${goos}_${arch}.go"
	"$GOFMT" -w "$ROOT/c2go_abi_anchor.go"
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
	opt_flags=(-O2)
	# The selftest qsort fixture has a large local array; the loop vectorizer
	# raises that slot to 16-byte alignment on amd64. It is test-only code, so
	# keep all scalar -O2 optimization while disabling both vectorizers there.
	[ "$arch" = amd64 ] && opt_flags+=(-fno-vectorize -fno-slp-vectorize)
	bcs=()
	for src in "$ROOT"/selftest/source/*.c; do
		b="$(printf '%s' "$src" | sed 's#[/.]#_#g')"
		"$CLANG" --target="$triple" -fc2go -fc2go-package="$MOD/selftest" -Xclang -fc2go-lto-prelink \
			"${opt_flags[@]}" -fno-math-errno \
			-emit-llvm -I "$RES" -I "$INC" \
			-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
		bcs+=("$tmp/${b}.${goos}_${arch}.bc")
	done
	asm="$tmp/selftest_${goos}_${arch}.s"; json="$tmp/selftest_${goos}_${arch}.json"
	"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
		--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
	cp "$asm" "$ROOT/selftest/selftest_${goos}_${arch}.s"
	normalize_asm_eof "$ROOT/selftest/selftest_${goos}_${arch}.s"
	case "$arch" in
		amd64) verify_amd64_stack_moves "$ROOT/selftest/selftest_${goos}_${arch}.s" ;;
		arm64) verify_arm64_link_register "$ROOT/selftest/selftest_${goos}_${arch}.s" ;;
	esac
	out="$tmp/out_selftest_${goos}_${arch}"; mkdir -p "$out"
	"$C2GOBIND" -goname="selftest_${goos}_${arch}" \
		-sidecar="$json" -out="$out" "$asm"
	cp "$out/selftest_${goos}_${arch}.go" "$ROOT/selftest/selftest_${goos}_${arch}.go"
	cp "$out/c2go_abi_anchor.go" "$ROOT/selftest/c2go_abi_anchor.go"
	"$GOFMT" -w "$ROOT/selftest/selftest_${goos}_${arch}.go"
	"$GOFMT" -w "$ROOT/selftest/c2go_abi_anchor.go"
	echo "  -> selftest/selftest_${goos}_${arch}.{go,s}"
done
echo "gen.sh: done."
