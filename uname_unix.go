// uname_unix.go — #675 C wave 2b: the uname bridge. The C-side struct
// utsname is the uniform musl shape (six 65-byte fields, <sys/utsname.h>);
// cUtsname mirrors it exactly. The per-OS unameRaw (uname_linux.go /
// uname_darwin.go) returns the six values as byte slices — the raw field
// types differ per OS ([65]int8 on linux, [256]byte on darwin, and darwin
// has no domainname) — and this shim truncates each to 64 chars + NUL.

//go:build unix

package libc

import "unsafe"

type cUtsname struct {
	sysname    [65]byte
	nodename   [65]byte
	release    [65]byte
	version    [65]byte
	machine    [65]byte
	domainname [65]byte
}

func utsFill(dst *[65]byte, src []byte) {
	n := 0
	for n < len(src) && n < 64 && src[n] != 0 {
		dst[n] = src[n]
		n++
	}
	// zero the whole tail (kernel semantics: the field is zero-filled,
	// not merely NUL-terminated)
	for i := n; i < 65; i++ {
		dst[i] = 0
	}
}

//go:linkname __c2go_syscall_uname
func __c2go_syscall_uname(buf unsafe.Pointer) int64 {
	fields, rc := unameRaw()
	if rc != 0 {
		return rc
	}
	c := (*cUtsname)(buf)
	utsFill(&c.sysname, fields[0])
	utsFill(&c.nodename, fields[1])
	utsFill(&c.release, fields[2])
	utsFill(&c.version, fields[3])
	utsFill(&c.machine, fields[4])
	utsFill(&c.domainname, fields[5])
	return 0
}
