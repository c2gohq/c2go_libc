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
    # string (mem/str/wcs; memcpy/memmove/memset/bzero + wmem* are compiler-intercepted)
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
    # wide-char classification / conversion (self-contained; multibyte DFA core deferred)
    ${MUSL_DIR}/src/ctype/iswalnum.c
    ${MUSL_DIR}/src/ctype/iswalpha.c
    ${MUSL_DIR}/src/ctype/iswblank.c
    ${MUSL_DIR}/src/ctype/iswcntrl.c
    ${MUSL_DIR}/src/ctype/iswdigit.c
    ${MUSL_DIR}/src/ctype/iswgraph.c
    ${MUSL_DIR}/src/ctype/iswlower.c
    ${MUSL_DIR}/src/ctype/iswprint.c
    ${MUSL_DIR}/src/ctype/iswpunct.c
    ${MUSL_DIR}/src/ctype/iswspace.c
    ${MUSL_DIR}/src/ctype/iswupper.c
    ${MUSL_DIR}/src/ctype/iswxdigit.c
    ${MUSL_DIR}/src/ctype/iswctype.c
    ${MUSL_DIR}/src/ctype/towctrans.c
    ${MUSL_DIR}/src/ctype/wctrans.c
    ${MUSL_DIR}/src/ctype/wcwidth.c
    ${MUSL_DIR}/src/ctype/wcswidth.c
    # search (intrusive linked-list; hsearch/tsearch deferred: need malloc glue)
    ${MUSL_DIR}/src/search/insque.c
)

# --- our independent C (no musl basis) ----------------------------------------
set(C2GO_OWN_SOURCES
    # e.g. ${CSRC_DIR}/io.c
)

# Include dirs handed to clang -fc2go for every TU above.
set(C2GO_INCLUDE_DIRS
    ${MUSL_DIR}/include
    # ${MUSL_DIR}/arch/<arch> is added per-target by the driver
    ${CSRC_DIR}
)

# NOTE: the custom command that drives clang -fc2go / c2go-lto / c2go-bind into the
# module root is added once the first TU migrates and the pipeline is wired to CMake.
