cmake_minimum_required(VERSION 3.20)
project(c2go_libc2 C)

# c2go-libc2 build manifest.
#
# Layout — the Go module lives at the REPO ROOT so the module path stays
# github.com/c2gohq/c2go_libc (underscore) and linknames keep the form
# "github.com/c2gohq/c2go_libc.Fopen" with NO subdir suffix (see MIGRATION_PLAN D2b):
#
#   <root>/        go.mod + the shipped Go module: hand-written + GENERATED *.go/*.s.
#   musl/          our musl fork. LOCAL SYMLINK (-> ../musl) for now; becomes a git
#                  submodule at push time. c2go changes edited IN PLACE on musl's own
#                  file layout (minimal diff). Excluded from the module zip (submodule
#                  contents never ship).
#   csrc/          our OWN C (no musl basis). Gets its own go.mod (nested module) so it is
#                  excluded from the module zip — the .c are build inputs; the shipped
#                  artifact is the generated *.go/*.s at the root.
#
# This file lists ONLY the TUs we actually build. The c2go toolchain (clang -fc2go ->
# c2go-lto -> c2go-bind) consumes this list and emits Go-linkable *.go/*.s into the root.

set(MUSL_DIR ${CMAKE_SOURCE_DIR}/musl)
set(CSRC_DIR ${CMAKE_SOURCE_DIR}/csrc)
set(GOMOD_ROOT ${CMAKE_SOURCE_DIR})   # generated *.go/*.s land here (= the module root)

# --- musl translation units we build (populated during migration) -------------
set(C2GO_MUSL_SOURCES
    # string (mem/str/wcs). memcpy/memmove/memset/bzero + wmem* ARE built here (musl
    # verbatim): they are the routing targets of the c2go-memcpy-typing pass (@llvm.mem*
    # -> the lowercase C symbol for an unmanaged dst; runtime.typedmemmove for a managed
    # one). musl's bodies are scalar (no aggregate copy / struct init), so they never
    # self-route. Only memmove carries a defensive no_builtin -- it is NOT in LoopIdiom's
    # name-guard list (unlike memset/memcpy/strlen); a no-op in this pipeline.
    ${MUSL_DIR}/src/string/memcpy.c
    ${MUSL_DIR}/src/string/memmove.c
    ${MUSL_DIR}/src/string/memset.c
    ${MUSL_DIR}/src/string/bzero.c
    ${MUSL_DIR}/src/string/wmemcpy.c
    ${MUSL_DIR}/src/string/wmemmove.c
    ${MUSL_DIR}/src/string/wmemset.c
    ${MUSL_DIR}/src/string/strlen.c
    ${MUSL_DIR}/src/string/memchr.c
    ${MUSL_DIR}/src/string/memcmp.c
    ${MUSL_DIR}/src/string/memrchr.c
    ${MUSL_DIR}/src/string/memccpy.c
    ${MUSL_DIR}/src/string/mempcpy.c
    ${MUSL_DIR}/src/string/memmem.c
    ${MUSL_DIR}/src/string/strnlen.c
    ${MUSL_DIR}/src/string/strcmp.c
    ${MUSL_DIR}/src/string/strncmp.c
    ${MUSL_DIR}/src/string/strcpy.c
    ${MUSL_DIR}/src/string/stpcpy.c
    ${MUSL_DIR}/src/string/strncpy.c
    ${MUSL_DIR}/src/string/stpncpy.c
    ${MUSL_DIR}/src/string/strlcpy.c
    ${MUSL_DIR}/src/string/strlcat.c
    ${MUSL_DIR}/src/string/strcat.c
    ${MUSL_DIR}/src/string/strncat.c
    ${MUSL_DIR}/src/string/strchr.c
    ${MUSL_DIR}/src/string/strchrnul.c
    ${MUSL_DIR}/src/string/strrchr.c
    ${MUSL_DIR}/src/string/strspn.c
    ${MUSL_DIR}/src/string/strcspn.c
    ${MUSL_DIR}/src/string/strpbrk.c
    ${MUSL_DIR}/src/string/strstr.c
    ${MUSL_DIR}/src/string/strcasestr.c
    ${MUSL_DIR}/src/string/strdup.c
    ${MUSL_DIR}/src/string/strndup.c
    ${MUSL_DIR}/src/string/strtok.c
    ${MUSL_DIR}/src/string/strtok_r.c
    ${MUSL_DIR}/src/string/strsep.c
    ${MUSL_DIR}/src/string/strcasecmp.c
    ${MUSL_DIR}/src/string/strncasecmp.c
    ${MUSL_DIR}/src/string/strerror_r.c
    ${MUSL_DIR}/src/string/strverscmp.c
    ${MUSL_DIR}/src/string/swab.c
    ${MUSL_DIR}/src/string/wcslen.c
    ${MUSL_DIR}/src/string/wcsnlen.c
    ${MUSL_DIR}/src/string/wcscpy.c
    ${MUSL_DIR}/src/string/wcpcpy.c
    ${MUSL_DIR}/src/string/wcsncpy.c
    ${MUSL_DIR}/src/string/wcpncpy.c
    ${MUSL_DIR}/src/string/wcscat.c
    ${MUSL_DIR}/src/string/wcsncat.c
    ${MUSL_DIR}/src/string/wcscmp.c
    ${MUSL_DIR}/src/string/wcsncmp.c
    ${MUSL_DIR}/src/string/wcschr.c
    ${MUSL_DIR}/src/string/wcsrchr.c
    ${MUSL_DIR}/src/string/wcsstr.c
    ${MUSL_DIR}/src/string/wcswcs.c
    ${MUSL_DIR}/src/string/wcsspn.c
    ${MUSL_DIR}/src/string/wcscspn.c
    ${MUSL_DIR}/src/string/wcspbrk.c
    ${MUSL_DIR}/src/string/wcstok.c
    ${MUSL_DIR}/src/string/wcsdup.c
    ${MUSL_DIR}/src/string/wmemchr.c
    ${MUSL_DIR}/src/string/wmemcmp.c
    ${MUSL_DIR}/src/string/wcscasecmp.c
    ${MUSL_DIR}/src/string/wcsncasecmp.c
    ${MUSL_DIR}/src/string/wcscasecmp_l.c
    ${MUSL_DIR}/src/string/wcsncasecmp_l.c
    # ctype
    ${MUSL_DIR}/src/ctype/isalpha.c
    ${MUSL_DIR}/src/ctype/isdigit.c
    ${MUSL_DIR}/src/ctype/isalnum.c
    ${MUSL_DIR}/src/ctype/isblank.c
    ${MUSL_DIR}/src/ctype/iscntrl.c
    ${MUSL_DIR}/src/ctype/isgraph.c
    ${MUSL_DIR}/src/ctype/islower.c
    ${MUSL_DIR}/src/ctype/isprint.c
    ${MUSL_DIR}/src/ctype/ispunct.c
    ${MUSL_DIR}/src/ctype/isspace.c
    ${MUSL_DIR}/src/ctype/isupper.c
    ${MUSL_DIR}/src/ctype/isxdigit.c
    ${MUSL_DIR}/src/ctype/isascii.c
    ${MUSL_DIR}/src/ctype/tolower.c
    ${MUSL_DIR}/src/ctype/toupper.c
    ${MUSL_DIR}/src/ctype/toascii.c
    # stdlib (integer arithmetic + ascii-to-number + bsearch)
    ${MUSL_DIR}/src/stdlib/abs.c
    ${MUSL_DIR}/src/stdlib/labs.c
    ${MUSL_DIR}/src/stdlib/llabs.c
    ${MUSL_DIR}/src/stdlib/imaxabs.c
    ${MUSL_DIR}/src/stdlib/div.c
    ${MUSL_DIR}/src/stdlib/ldiv.c
    ${MUSL_DIR}/src/stdlib/lldiv.c
    ${MUSL_DIR}/src/stdlib/imaxdiv.c
    ${MUSL_DIR}/src/stdlib/bsearch.c
    ${MUSL_DIR}/src/stdlib/atoi.c
    ${MUSL_DIR}/src/stdlib/atol.c
    ${MUSL_DIR}/src/stdlib/atoll.c
    # prng
    ${MUSL_DIR}/src/prng/rand.c
    ${MUSL_DIR}/src/prng/rand_r.c
    # NOTE: wide-char ctype (iswalpha/towlower/wctype/wcwidth/... + _l) is NOT built
    # verbatim from musl. Its big Unicode tables (alpha.h/casemap.h ~39KB) are delegated
    # to Go's unicode package — the agreed design (mirrors iconv). It lives in
    # ${CSRC_DIR}/wctype.c (C/Go hybrid) + wctype.go, listed under C2GO_OWN_SOURCES below.
    # search (intrusive linked-list; hsearch/tsearch deferred: need malloc glue)
    ${MUSL_DIR}/src/search/insque.c
)

# --- our independent C (no musl basis) ----------------------------------------
set(C2GO_OWN_SOURCES
    # wide-char ctype: C/Go hybrid. ASCII in C; the big Unicode tables (alpha/casemap)
    # are delegated to Go's unicode package via wctype.go (agreed design — a large
    # lookup table is Go's to own, mirrors iconv). NOT verbatim musl: that would vendor
    # musl's ~39KB tables. Vendors only the small ~5KB width tables (nonspacing.h/wide.h,
    # in csrc/) since Go has no East-Asian-Width data. See MIGRATION_PLAN.
    ${CSRC_DIR}/wctype.c
    # pthread C glue: the three fp-call trampolines (__c2go_pthread_run{,_void,_dtor} —
    # a pthread start routine / once-init / TSD destructor is a c2go fp, so it must be
    # CALLED in C, the qsort.c pattern) plus the #664 attr-object lifecycle bodies
    # (mutexattr/condattr/rwlockattr init/destroy/settype, pthread_atfork). The Go side
    # of pthreads (mutex/cond/rwlock/sem/key/thread state) is the glue in *.go at root.
    ${CSRC_DIR}/pthread.c
    # raw fd layer: thin C wrappers (read/write/open/close/lseek/fcntl/dup2/dup3/unlink/
    # rmdir/rename) that marshal into one Go syscall shim each (io.go + io_{darwin,linux,
    # windows}.go). posix and windows split; the windows TU delegates std fds 0/1/2 to
    # the Go std-descriptor layer (stdio_std*.go) and the rest to the msvcrt CRT.
    # Unblocks the stdio (FILE*) layer next.
    ${CSRC_DIR}/io_posix.c
    ${CSRC_DIR}/io_windows.c
    # stdio (FILE*) layer: one adapted blob (musl-derived, c2go-modified — the FILE*
    # buffering + printf/scanf engines vfprintf/vfscanf/__floatscan/__intscan/fmt_fp are
    # self-contained file-local statics). Owns the number-conversion family (strtol/strtod/
    # wcsto*) too, so never also add musl strtol.c/strtod.c (would duplicate). Per-FILE
    # lock + FILE handle bridge to Go is stdio.go (root). bits/stdio_impl.h supplies the
    # FILE struct with the c2go lockid field.
    ${CSRC_DIR}/stdio.c
)

# Include dirs handed to clang -fc2go for every TU above.
set(C2GO_INCLUDE_DIRS
    ${MUSL_DIR}/include
    # ${MUSL_DIR}/arch/<arch> is added per-target by the driver
    ${CSRC_DIR}
)

# NOTE: the custom command that drives clang -fc2go / c2go-lto / c2go-bind into the
# module root is added once the first TU migrates and the pipeline is wired to CMake.
