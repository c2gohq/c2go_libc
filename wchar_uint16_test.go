//go:build windows && amd64

package libc

// Windows uses a 16-bit wchar_t.
type testWchar = uint16
