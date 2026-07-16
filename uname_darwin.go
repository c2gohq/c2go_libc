// uname_darwin.go — the darwin side of the uname bridge: x/sys unix.Uname
// (sysctl-backed; darwin's syscall package has no Uname) fills [256]byte
// fields. darwin has no domainname — delivered empty (documented in
// <sys/utsname.h>).

//go:build darwin

package libc

import "golang.org/x/sys/unix"

func unameField(f []byte) []byte {
	for i, c := range f {
		if c == 0 {
			return f[:i]
		}
	}
	return f
}

func unameRaw() ([6][]byte, int64) {
	var u unix.Utsname
	if err := unix.Uname(&u); err != nil {
		return [6][]byte{}, errnoRet(err)
	}
	return [6][]byte{
		unameField(u.Sysname[:]),
		unameField(u.Nodename[:]),
		unameField(u.Release[:]),
		unameField(u.Version[:]),
		unameField(u.Machine[:]),
		nil, /* no domainname on darwin */
	}, 0
}
