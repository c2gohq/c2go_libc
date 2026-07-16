// resource_unix.go — #675 C wave 2b: getrusage/getrlimit bridges. The C-side
// structs are the uniform musl shapes (<sys/resource.h>); cRusage mirrors
// struct rusage exactly (two {i64,i64} timevals + 14 longs + 16 reserved).
// Field values stay kernel-native (ru_maxrss: KiB on linux, BYTES on darwin;
// darwin leaves most other ru_* fields zero) — documented in the header, not
// normalized here. RLIMIT_* selectors arrive as the target's native values
// from the header and pass straight through; RLIM_INFINITY likewise matches
// the host representation on both sides, so no musl-style FIX() mapping is
// needed (Go's Getrlimit is prlimit64-backed on linux).

//go:build unix

package libc

import (
	"syscall"
	"unsafe"
)

type cTimeval struct{ sec, usec int64 }

type cRusage struct {
	utime, stime cTimeval
	maxrss, ixrss, idrss, isrss     int64
	minflt, majflt, nswap           int64
	inblock, oublock                int64
	msgsnd, msgrcv, nsignals        int64
	nvcsw, nivcsw                   int64
	_                               [16]int64
}

//go:linkname __c2go_syscall_getrusage
func __c2go_syscall_getrusage(who int32, buf unsafe.Pointer) int64 {
	var ru syscall.Rusage
	if err := syscall.Getrusage(int(who), &ru); err != nil {
		return errnoRet(err)
	}
	c := (*cRusage)(buf)
	c.utime = cTimeval{int64(ru.Utime.Sec), int64(ru.Utime.Usec)}
	c.stime = cTimeval{int64(ru.Stime.Sec), int64(ru.Stime.Usec)}
	c.maxrss = int64(ru.Maxrss)
	c.ixrss = int64(ru.Ixrss)
	c.idrss = int64(ru.Idrss)
	c.isrss = int64(ru.Isrss)
	c.minflt = int64(ru.Minflt)
	c.majflt = int64(ru.Majflt)
	c.nswap = int64(ru.Nswap)
	c.inblock = int64(ru.Inblock)
	c.oublock = int64(ru.Oublock)
	c.msgsnd = int64(ru.Msgsnd)
	c.msgrcv = int64(ru.Msgrcv)
	c.nsignals = int64(ru.Nsignals)
	c.nvcsw = int64(ru.Nvcsw)
	c.nivcsw = int64(ru.Nivcsw)
	return 0
}

//go:linkname __c2go_syscall_getrlimit
func __c2go_syscall_getrlimit(res int32, buf unsafe.Pointer) int64 {
	var rl syscall.Rlimit
	if err := syscall.Getrlimit(int(res), &rl); err != nil {
		return errnoRet(err)
	}
	*(*[2]uint64)(buf) = [2]uint64{rl.Cur, rl.Max}
	return 0
}
