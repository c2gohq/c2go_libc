// uname_linux.go — the linux side of the uname bridge: syscall.Uname fills
// a syscall.Utsname of [65]int8 fields (including Domainname).

//go:build linux

package libc

import "syscall"

func unameField(f []int8) []byte {
	out := make([]byte, 0, len(f))
	for _, c := range f {
		if c == 0 {
			break
		}
		out = append(out, byte(c))
	}
	return out
}

func unameRaw() ([6][]byte, int64) {
	var u syscall.Utsname
	if err := syscall.Uname(&u); err != nil {
		return [6][]byte{}, errnoRet(err)
	}
	return [6][]byte{
		unameField(u.Sysname[:]),
		unameField(u.Nodename[:]),
		unameField(u.Release[:]),
		unameField(u.Version[:]),
		unameField(u.Machine[:]),
		unameField(u.Domainname[:]),
	}, 0
}
