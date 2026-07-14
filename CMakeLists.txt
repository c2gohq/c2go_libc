cmake_minimum_required(VERSION 3.20)
project(c2go_libc2 C)

# c2go-libc2 build manifest.
#
# Layout:
#   musl/    upstream musl 1.2.6 as a git submodule; our c2go changes live on its
#            `c2go` branch (edited in place on musl's own file layout). Headers stay
#            under musl/include (+ musl/arch/<a>). Unused musl files are NOT deleted.
#   csrc/    our OWN C with no musl basis (Go-bridge shims, runtime glue, per-OS).
#   golib/   the Go module (go.mod + hand + GENERATED .go/.s). Kept in its own subtree
#            so a `go get` only pulls the Go files, not musl/csrc (small proxy zip).
#
# This file lists ONLY the translation units we actually build (the musl files we
# modified/need + all of csrc). The c2go toolchain (clang -fc2go -> c2go-lto ->
# c2go-bind) consumes this list and emits the Go-linkable objects into golib/.

set(MUSL_DIR ${CMAKE_SOURCE_DIR}/musl)
set(CSRC_DIR ${CMAKE_SOURCE_DIR}/csrc)
set(GOLIB_DIR ${CMAKE_SOURCE_DIR}/golib)

# --- musl translation units we build (populated during migration) -------------
set(C2GO_MUSL_SOURCES
    # e.g. ${MUSL_DIR}/src/math/exp.c
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

# NOTE: the custom command that drives clang -fc2go / c2go-lto / c2go-bind into
# golib/ is added once the first TU migrates and the pipeline is wired to CMake.
