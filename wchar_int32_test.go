//go:build darwin || (linux && amd64)

package libc

// testWchar follows the target C ABI used by the generated binding.
type testWchar = int32
