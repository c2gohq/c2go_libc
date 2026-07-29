//go:build linux && arm64

package libc

// AArch64 Linux uses an unsigned 32-bit wchar_t.
type testWchar = uint32
