// Package dl implements the external native-call boundary for c2go
// `unmanaged extern` symbols. Function addresses may come from load-time
// //go:cgo_import_dynamic bindings or from Dlopen and Dlsym.
//
// On Unix, native calls use PureGo's fake-cgo machinery and runtime.cgocall.
// The class-separated Call path is pinned to the exact PureGo private ABI in
// this module's go.mod. Windows uses LoadLibrary, GetProcAddress, and
// syscall.SyscallN instead of the Unix trampoline.
//
// This package is intended for c2go-bind-generated wrappers and explicit calls
// into host libraries. The top-level c2go-libc package must not import it:
// managed libc implementations run in-process and should not pay an external
// cgocall transition. isolation_test.go enforces that boundary.
package dl
