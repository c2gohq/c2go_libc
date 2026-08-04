// Package c2goabi publishes the compile-time contracts shared by C2Go-generated
// packages, c2go-libc, and the active Go toolchain.
//
// It is intentionally dependency-free. Build-tagged provider files admit only
// Go toolchains whose assembly/linker/runtime contract has been validated by
// the complete C2Go toolchain test matrix.
package c2goabi
