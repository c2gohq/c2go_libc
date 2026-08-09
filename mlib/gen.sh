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

verify_arm64_write_barrier_loads() {
	local asm_path="$1"
	local unsafe_pattern='^[[:space:]]*MOVD[[:space:]]+runtime·writeBarrier\(SB\)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "mlib/gen.sh: invalid arm64 runtime.writeBarrier value load in $asm_path" >&2
		grep -nE "$unsafe_pattern" "$asm_path" >&2
		exit 1
	fi
}

verify_no_unmanaged_allocator_calls() {
	local asm_path="$1"
	local unsafe_pattern='CALL[[:space:]]+[^[:space:]]*·(Malloc|Calloc|Realloc|Reallocarray|AlignedAlloc|PosixMemalign|Free|malloc|calloc|realloc|reallocarray|aligned_alloc|posix_memalign|free)\(SB\)'
	if grep -Eq "$unsafe_pattern" "$asm_path"; then
		echo "mlib/gen.sh: unmanaged allocator call in $asm_path" >&2
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

verify_function_call_count() {
	local asm_path="$1"
	local symbol="$2"
	local callee="$3"
	local minimum="$4"
	local description="$5"
	local count
	count="$(awk -v prefix="TEXT ·${symbol}(SB)" -v call="CALL ·${callee}(SB)" '
		index($0, prefix) == 1 { in_function=1; next }
		in_function && /^TEXT / { exit }
		in_function && index($0, call) { count++ }
		END { print count+0 }
	' "$asm_path")"
	if [ "$count" -lt "$minimum" ]; then
		echo "mlib/gen.sh: $description has $count calls, want at least $minimum in $asm_path" >&2
		exit 1
	fi
}

verify_function_symbol_call_count() {
	local asm_path="$1"
	local symbol="$2"
	local callee="$3"
	local minimum="$4"
	local description="$5"
	local count
	count="$(awk -v prefix="TEXT ·${symbol}(SB)" -v suffix="·${callee}(SB)" '
		index($0, prefix) == 1 { in_function=1; next }
		in_function && /^TEXT / { exit }
		in_function && /CALL / && index($0, suffix) { count++ }
		END { print count+0 }
	' "$asm_path")"
	if [ "$count" -lt "$minimum" ]; then
		echo "mlib/gen.sh: $description has $count calls, want at least $minimum in $asm_path" >&2
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
	verify_function_write_barrier "$asm_path" mlib_file_allocate \
		"managed FILE buffer ownership"
	verify_function_write_barrier "$asm_path" mlib_store_state_pointer \
		"managed FILE state publication"
	verify_function_write_barrier "$asm_path" mlib_store_line_pointer \
		"managed memory-stream result publication"
	verify_function_call_count "$asm_path" mlib_fmemopen \
		mlib_store_state_pointer 1 "managed fmemopen buffer ownership"
	verify_function_call_count "$asm_path" mlib_open_memstream \
		mlib_store_state_pointer 3 "managed open_memstream roots"
	verify_function_call_count "$asm_path" mlib_open_memstream \
		mlib_store_line_pointer 1 "managed open_memstream initial result"
	verify_function_call_count "$asm_path" mlib_memstream_write \
		mlib_store_state_pointer 1 "managed memstream buffer growth"
	verify_function_call_count "$asm_path" mlib_memstream_write \
		mlib_store_line_pointer 1 "managed memstream result replacement"
	verify_function_call_count "$asm_path" mlib_fopencookie \
		mlib_store_state_pointer 1 "managed fopencookie state ownership"
	verify_function_call_count "$asm_path" mlib_popen \
		mlib_store_state_pointer 1 "managed popen process ownership"
	verify_function_write_barrier "$asm_path" mlib_stdfile_store \
		"managed standard-stream rooting"
	verify_function_write_barrier "$asm_path" mlib_ofl_add \
		"managed FILE open-list insertion"
	verify_function_write_barrier "$asm_path" mlib_ofl_remove \
		"managed FILE open-list removal"
	verify_function_call_count "$asm_path" mlib_ofl_remove \
		mlib_stdfile_store 1 "managed standard-stream root retirement"
	verify_function_write_barrier "$asm_path" mlib_clear_file_pointer \
		"managed FILE link retirement"
	verify_function_write_barrier "$asm_path" mlib_clear_buffer_pointer \
		"managed FILE buffer retirement"
	verify_function_write_barrier "$asm_path" mlib_clear_state_pointer \
		"managed FILE object retirement"
	verify_function_call_count "$asm_path" mlib_file_clear_links \
		mlib_clear_file_pointer 2 "managed FILE link clearing"
	verify_function_call_count "$asm_path" mlib_fclose \
		mlib_clear_buffer_pointer 1 "managed FILE buffer release"
	verify_function_call_count "$asm_path" mlib_fclose \
		mlib_clear_state_pointer 5 "managed FILE object, process, slot, and lock release"
	verify_function_write_barrier "$asm_path" mlib_tnode_clear_key \
		"managed tree key retirement"
	verify_function_write_barrier "$asm_path" mlib_tnode_clear_child \
		"managed tree link retirement"
	verify_function_write_barrier "$asm_path" mlib_tsearch \
		"managed tree insertion and rotation"
	verify_function_write_barrier "$asm_path" mlib_tdelete \
		"managed tree unlink and rebalance"
	verify_function_call_count "$asm_path" mlib_tdelete \
		mlib_tnode_clear_key 1 "managed tree deleted-key retirement"
	verify_function_call_count "$asm_path" mlib_tdelete \
		mlib_tnode_clear_child 2 "managed tree deleted-link retirement"
	verify_function_call_count "$asm_path" mlib_tdestroy_node \
		mlib_tnode_clear_key 1 "managed tree destroy-key retirement"
	verify_function_call_count "$asm_path" mlib_tdestroy_node \
		mlib_tnode_clear_child 2 "managed tree destroy-link retirement"
	verify_function_call_count "$asm_path" mlib_hsearch_resize \
		_c2go_typedmemmove 1 "managed hash rehash copies"
	verify_function_write_barrier "$asm_path" mlib_hsearch_resize \
		"managed hash table replacement"
	verify_function_call_count "$asm_path" mlib_hsearch_r \
		_c2go_typedmemmove 1 "managed hash entry publication"
	verify_function_write_barrier "$asm_path" mlib_hsearch_r \
		"managed hash result publication"
	verify_function_write_barrier "$asm_path" mlib_hsearch_clear_key \
		"managed hash failed-key retirement"
	verify_function_write_barrier "$asm_path" mlib_hsearch_clear_data \
		"managed hash failed-data retirement"
	verify_function_write_barrier "$asm_path" mlib_hdestroy_r \
		"managed hash table retirement"
	verify_function_write_barrier "$asm_path" mlib_insque \
		"managed queue insertion"
	verify_function_write_barrier "$asm_path" mlib_queue_clear_link \
		"managed queue link retirement"
	verify_function_call_count "$asm_path" mlib_insque \
		mlib_queue_clear_link 2 "managed queue head reset"
	verify_function_call_count "$asm_path" mlib_remque \
		mlib_queue_clear_link 2 "managed queue removed-link retirement"
	verify_function_write_barrier "$asm_path" mlib_remque \
		"managed queue unlink"
	verify_function_write_barrier "$asm_path" mlib_regex_store_root \
		"managed regex root publication"
	verify_function_write_barrier "$asm_path" mlib_regex_clear_root \
		"managed regex root retirement"
	verify_function_write_barrier "$asm_path" __mlib_regcomp_impl \
		"managed regex TNFA publication"
	verify_function_call_count "$asm_path" mlib_regcomp \
		mlib_regex_store_root 1 "managed regex arena publication"
	verify_function_call_count "$asm_path" mlib_regcomp \
		mlib_regex_clear_root 3 "managed regex compile cleanup"
	verify_function_call_count "$asm_path" mlib_regfree \
		mlib_regex_clear_root 2 "managed regex arena retirement"
	verify_function_call_count "$asm_path" mlib_regcomp \
		regexArenaNew 1 "managed regex persistent arena creation"
	verify_function_call_count "$asm_path" mlib_regexec \
		regexArenaNew 1 "managed regex per-call arena creation"
	verify_function_symbol_call_count "$asm_path" mlib_strdup \
		GCMalloc 1 "managed strdup allocation"
	verify_function_symbol_call_count "$asm_path" mlib_strndup \
		GCMalloc 1 "managed strndup allocation"
	verify_function_symbol_call_count "$asm_path" mlib_wcsdup \
		GCMalloc 1 "managed wcsdup allocation"
	verify_function_symbol_call_count "$asm_path" mlib_vasprintf \
		GCMalloc 1 "managed vasprintf allocation"
	verify_function_write_barrier "$asm_path" mlib_store_formatted_pointer \
		"managed vasprintf result stores"
	verify_function_call_count "$asm_path" mlib_vasprintf \
		mlib_store_formatted_pointer 2 "managed vasprintf result retirement and publication"
	verify_function_call_count "$asm_path" mlib_asprintf \
		mlib_vasprintf 1 "managed asprintf variadic forwarding"
	verify_function_symbol_call_count "$asm_path" mlib_realpath \
		GCMalloc 1 "managed realpath allocation"
	verify_function_symbol_call_count "$asm_path" mlib_getcwd \
		GCMalloc 1 "managed getcwd allocation"
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
	"$ROOT/csrc/mlib/stdio.c"
	"$ROOT/csrc/mlib/thread.c"
	"$ROOT/csrc/mlib/search.c"
	"$ROOT/csrc/mlib/regex.c"
	"$ROOT/musl/src/regex/regcomp.c"
	"$ROOT/musl/src/regex/regexec.c"
	"$ROOT/musl/src/regex/tre-mem.c"
	# Keep additive families last so their function/label numbering does not
	# rewrite the existing large TRE assembly on every regeneration.
	"$ROOT/csrc/mlib/string.c"
	"$ROOT/csrc/mlib/wstring.c"
	"$ROOT/csrc/mlib/asprintf.c"
	"$ROOT/csrc/mlib/realpath.c"
	"$ROOT/csrc/mlib/getcwd.c"
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
			compile=(
				"$CLANG" --target="$triple" -fc2go
				-fc2go-package="$MOD/mlib" -Xclang -fc2go-lto-prelink
				"${opt_flags[@]}"
			)
			case "$src" in
				"$ROOT/musl/src/regex/regcomp.c"|\
				"$ROOT/musl/src/regex/regexec.c"|\
				"$ROOT/musl/src/regex/tre-mem.c")
					compile+=( -DC2GO_MLIB_REGEX_BUILD=1 )
					;;
			esac
			compile+=(
				-fno-math-errno -emit-llvm -I "$RES" -I "$INC"
				-c -o "$tmp/${b}.${goos}_${arch}.bc" "$src"
			)
			"${compile[@]}"
			bcs+=("$tmp/${b}.${goos}_${arch}.bc")
		done
		asm="$tmp/libc_${goos}_${arch}.s"
		json="$tmp/libc_${goos}_${arch}.json"
		"$C2GOLTO" --c2go-lto-inline --c2go-escape-nonfatal \
			--c2go-emit-asm="$asm" --c2go-emit-manifest="$json" "${bcs[@]}"
		normalize_asm_eof "$asm"
		case "$arch" in
			amd64) verify_amd64_stack_moves "$asm" ;;
			arm64)
				verify_arm64_link_register "$asm"
				verify_arm64_write_barrier_loads "$asm"
				;;
		esac
		verify_no_unmanaged_allocator_calls "$asm"
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
	local string_selftest
	local asprintf_selftest
	local realpath_selftest
	local getcwd_selftest
	local stdio_retire_selftest
	local dirent_retire_selftest
	local sync_retire_selftest
	case "$mode" in
		namespaced)
			string_selftest=mlib_string_prefixed_selftest
			asprintf_selftest=mlib_asprintf_prefixed_selftest
			realpath_selftest=mlib_realpath_prefixed_selftest
			getcwd_selftest=mlib_getcwd_prefixed_selftest
			stdio_retire_selftest=mlib_stdio_retire_prefixed_selftest
			dirent_retire_selftest=mlib_dirent_retire_prefixed_selftest
			sync_retire_selftest=mlib_sync_retire_prefixed_selftest
			;;
		unprefixed)
			string_selftest=mlib_string_unprefixed_selftest
			asprintf_selftest=mlib_asprintf_unprefixed_selftest
			realpath_selftest=mlib_realpath_unprefixed_selftest
			getcwd_selftest=mlib_getcwd_unprefixed_selftest
			stdio_retire_selftest=mlib_stdio_retire_unprefixed_selftest
			dirent_retire_selftest=mlib_dirent_retire_unprefixed_selftest
			sync_retire_selftest=mlib_sync_retire_unprefixed_selftest
			;;
		*) echo "mlib/gen.sh: unknown namespace mode $mode" >&2; exit 1 ;;
	esac
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
			arm64)
				verify_arm64_link_register "$test_dir/selftest_${goos}_${arch}.s"
				verify_arm64_write_barrier_loads "$test_dir/selftest_${goos}_${arch}.s"
				;;
		esac
		verify_function_write_barrier "$test_dir/selftest_${goos}_${arch}.s" \
			c2go_mlib_cookie_write "managed cookie callback publication"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$string_selftest" mlib_strdup 1 \
			"$mode standard/namespaced strdup routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$string_selftest" mlib_strndup 1 \
			"$mode standard/namespaced strndup routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$string_selftest" mlib_wcsdup 1 \
			"$mode standard/namespaced wcsdup routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$asprintf_selftest" mlib_asprintf 2 \
			"$mode standard/namespaced asprintf routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			c2go_mlib_asprintf_test_vcall mlib_vasprintf 1 \
			"$mode standard/namespaced vasprintf routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$realpath_selftest" mlib_realpath 4 \
			"$mode standard/namespaced realpath routing"
		verify_function_write_barrier "$test_dir/selftest_${goos}_${arch}.s" \
			"$realpath_selftest" "managed realpath owner retirement and publication"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$getcwd_selftest" mlib_getcwd 4 \
			"$mode standard/namespaced getcwd routing"
		verify_function_write_barrier "$test_dir/selftest_${goos}_${arch}.s" \
			"$getcwd_selftest" "managed getcwd owner retirement and publication"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$stdio_retire_selftest" mlib_stdfile 2 \
			"$mode managed standard-stream lookup routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$stdio_retire_selftest" mlib_fclose 2 \
			"$mode managed standard-stream close routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$dirent_retire_selftest" mlib_opendir 1 \
			"$mode managed DIR open routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$dirent_retire_selftest" mlib_closedir 1 \
			"$mode managed DIR close routing"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$dirent_retire_selftest" mlib_scandir 1 \
			"$mode managed scandir routing"
		verify_function_write_barrier "$test_dir/selftest_${goos}_${arch}.s" \
			"$dirent_retire_selftest" \
			"$mode managed DIR/scandir owner retirement"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" GCMalloc 5 \
			"$mode managed synchronization carrier allocation"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" SemDestroy 1 \
			"$mode managed semaphore state retirement"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" PthreadMutexDestroy 1 \
			"$mode managed mutex state retirement"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" PthreadCondDestroy 1 \
			"$mode managed condition state retirement"
		verify_function_symbol_call_count "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" PthreadRWLockDestroy 1 \
			"$mode managed rwlock state retirement"
		verify_function_write_barrier "$test_dir/selftest_${goos}_${arch}.s" \
			"$sync_retire_selftest" \
			"$mode managed synchronization owner retirement"
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
