#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Regenerate the compiled-C mlib header selftests for every release target.
set -euo pipefail

MLIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$MLIB_DIR/.." && pwd)"
MOD=github.com/c2gohq/c2go_libc
CLANG="${CLANG:-/Users/dexter/Downloads/llvm-project/build/bin/clang}"
C2GOLTO="${C2GOLTO:-/Users/dexter/Downloads/llvm-project/build/bin/c2go-lto}"
C2GOBIND="${C2GOBIND:-/Users/dexter/Downloads/c2go/c2go-bind/c2go-bind}"
GOFMT="${GOFMT:-gofmt}"
RES="$("$CLANG" -print-resource-dir)/include"
INC="$ROOT/csrc/include"

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

verify_amd64_stack_moves() {
	local asm_path="$1"
	local unsafe_pattern='^[[:space:]]*(V?MOVAPS|V?MOVAPD|MOVO|VMOVDQA(32|64)?)[[:space:]].*\((BP|SP)\)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "mlib/gen.sh: unsafe aligned amd64 stack move in $asm_path" >&2
		grep -nE "$unsafe_pattern" "$asm_path" >&2
		exit 1
	fi
}

verify_arm64_link_register() {
	local asm_path="$1"
	local unsafe_pattern='(^|[^[:alnum:]_])R30([^[:alnum:]_]|$)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "mlib/gen.sh: allocated arm64 link register in $asm_path" >&2
		grep -nE "$unsafe_pattern" "$asm_path" >&2
		exit 1
	fi
}

verify_function_write_barrier() {
	local asm_path="$1"
	local symbol="$2"
	local description="$3"
	if ! awk -v prefix="TEXT ·${symbol}(SB)" '
		index($0, prefix) == 1 { in_function=1; next }
		in_function && /^TEXT / { exit }
		in_function && /_c2go_writePtr\(SB\)/ { found=1; exit }
		END { exit !found }
	' "$asm_path"; then
		echo "mlib/gen.sh: $description lost write barriers in $asm_path" >&2
		exit 1
	fi
}

verify_managed_write_barriers() {
	local asm_path="$1"
	verify_function_write_barrier "$asm_path" mlib_opendir \
		"managed DIR state installation"
	verify_function_write_barrier "$asm_path" mlib_closedir \
		"managed DIR state removal"
	verify_function_write_barrier "$asm_path" mlib_scandir_sort \
		"managed scandir pointer swaps"
	verify_function_write_barrier "$asm_path" mlib_scandir_store_result \
		"managed scandir result publication"
	verify_function_write_barrier "$asm_path" mlib_glob_sort \
		"managed glob pointer swaps"
	verify_function_write_barrier "$asm_path" mlib_glob_store_paths \
		"managed glob carrier updates"
}

TARGETS=(
	"darwin:arm64:aarch64-apple-darwin"
	"darwin:amd64:x86_64-apple-darwin"
	"linux:arm64:aarch64-unknown-linux-goabi"
	"linux:amd64:x86_64-unknown-linux-goabi"
	"windows:amd64:x86_64-pc-windows-goabi:msvcrt"
)

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

LIB_SOURCES=(
	"$ROOT/csrc/mlib/dirent.c"
	"$ROOT/csrc/mlib/ftw.c"
	"$ROOT/csrc/mlib/glob.c"
	"$ROOT/csrc/mlib/scandir.c"
)

# Build the selectively-instantiated C part of package mlib once per target.
# The implementation always exports mlib_* symbols; unprefixed headers route
# standard C names to those symbols instead of creating a second library copy.
generate_library() {
	echo "== mlib library ($MOD/mlib) =="
	for t in "${TARGETS[@]}"; do
		IFS=: read -r goos arch triple lib <<<"$t"
		opt_flags=(-O2)
		[ "$arch" = amd64 ] && opt_flags+=(-fno-slp-vectorize)
		bcs=()
		for src in "${LIB_SOURCES[@]}"; do
			b="$(printf '%s' "$src" | sed 's#[/.]#_#g')"
			"$CLANG" --target="$triple" -fc2go -fc2go-package="$MOD/mlib" -Xclang -fc2go-lto-prelink \
				"${opt_flags[@]}" -fno-math-errno -emit-llvm -I "$RES" -I "$INC" \
				-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
			bcs+=("$tmp/${b}.${goos}_${arch}.bc")
		done
		asm="$tmp/libc_${goos}_${arch}.s"
		json="$tmp/libc_${goos}_${arch}.json"
		"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
			--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
		normalize_asm_eof "$asm"
		case "$arch" in
			amd64) verify_amd64_stack_moves "$asm" ;;
			arm64) verify_arm64_link_register "$asm" ;;
		esac
		verify_managed_write_barriers "$asm"
		out="$tmp/out_lib_${goos}_${arch}"
		mkdir -p "$out"
		"$C2GOBIND" -pkgname=mlib -goname="libc_${goos}_${arch}" \
			-sidecar="$json" -out="$out" ${lib:+-l "$lib"} "$asm"
		cp "$out/libc_${goos}_${arch}.go" "$MLIB_DIR/libc_${goos}_${arch}.go"
		cp "$out/libc_${goos}_${arch}.s" "$MLIB_DIR/libc_${goos}_${arch}.s"
		cp "$out/c2go_abi_anchor.go" "$MLIB_DIR/c2go_abi_anchor.go"
		cp "$out/version_error_too_old.go" "$MLIB_DIR/version_error_too_old.go"
		"$GOFMT" -w "$MLIB_DIR/libc_${goos}_${arch}.go"
		"$GOFMT" -w "$MLIB_DIR/c2go_abi_anchor.go" "$MLIB_DIR/version_error_too_old.go"
		echo "  -> libc_${goos}_${arch}.{go,s}"
	done
}

# Choosing C2GO_MLIB_UNPREFIXED changes the standard-C symbol routes for the
# whole LTO package. Keep the default and replacement modes in distinct test
# packages, exactly as downstream builds must do.
generate_mode() {
	local mode="$1"
	local test_dir="$MLIB_DIR/selftest/$mode"
	local test_mod="$MOD/mlib/selftest/$mode"
	echo "== mlib/selftest/$mode ($test_mod) =="
	for t in "${TARGETS[@]}"; do
		IFS=: read -r goos arch triple lib <<<"$t"
		opt_flags=(-O2)
		[ "$arch" = amd64 ] && opt_flags+=(-fno-slp-vectorize)
		bcs=()
		for src in "$test_dir"/source/*.c; do
			b="$(printf '%s' "$src" | sed 's#[/.]#_#g')"
			"$CLANG" --target="$triple" -fc2go -fc2go-package="$test_mod" -Xclang -fc2go-lto-prelink \
				"${opt_flags[@]}" -fno-math-errno -emit-llvm -I "$RES" -I "$INC" \
				-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
			bcs+=("$tmp/${b}.${goos}_${arch}.bc")
		done
		asm="$tmp/mlib_${mode}_${goos}_${arch}.s"
		json="$tmp/mlib_${mode}_${goos}_${arch}.json"
		"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
			--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
		cp "$asm" "$test_dir/selftest_${goos}_${arch}.s"
		normalize_asm_eof "$test_dir/selftest_${goos}_${arch}.s"
		case "$arch" in
			amd64) verify_amd64_stack_moves "$test_dir/selftest_${goos}_${arch}.s" ;;
			arm64) verify_arm64_link_register "$test_dir/selftest_${goos}_${arch}.s" ;;
		esac
		out="$tmp/out_${mode}_${goos}_${arch}"
		mkdir -p "$out"
		"$C2GOBIND" -pkgname="$mode" -goname="selftest_${goos}_${arch}" \
			-sidecar="$json" -out="$out" ${lib:+-l "$lib"} "$asm"
		cp "$out/selftest_${goos}_${arch}.go" "$test_dir/selftest_${goos}_${arch}.go"
		cp "$out/c2go_abi_anchor.go" "$test_dir/c2go_abi_anchor.go"
		cp "$out/version_error_too_old.go" "$test_dir/version_error_too_old.go"
		"$GOFMT" -w "$test_dir/selftest_${goos}_${arch}.go"
		"$GOFMT" -w "$test_dir/c2go_abi_anchor.go" "$test_dir/version_error_too_old.go"
		echo "  -> $mode/selftest_${goos}_${arch}.{go,s}"
	done
}

generate_library
generate_mode namespaced
generate_mode unprefixed
echo "mlib/gen.sh: done."
