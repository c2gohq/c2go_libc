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
    # string
    ${MUSL_DIR}/src/string/strlen.c
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
