// conf_bridge.go — #675 (user decision 1+3): cross-platform conf shims.
// Backed entirely by platform-independent Go primitives, so no build tag —
// windows gets them too (gethostname is in MinGW via winsock; the other
// three are provided under the relaxed "Go primitive honestly delivers the
// semantics" line). C wrappers in source/conf.c.

package libc

import (
	"crypto/rand"
	"os"
	"runtime"
	"syscall"
	"unsafe"
)

//go:linkname __c2go_gethostname
func __c2go_gethostname(buf *byte, n uint64) int64 {
	h, err := os.Hostname()
	if err != nil {
		return -int64(syscall.EIO)
	}
	b := unsafe.Slice(buf, n)
	if uint64(len(h)+1) > n {
		copy(b, h[:n-1]) // POSIX: truncation is implementation-defined; NUL-terminate
		b[n-1] = 0
		return -int64(syscall.ENAMETOOLONG)
	}
	copy(b, h)
	b[len(h)] = 0
	return 0
}

//go:linkname __c2go_getentropy
func __c2go_getentropy(buf *byte, n uint64) int64 {
	if n > 256 {
		return -int64(syscall.EIO) // POSIX getentropy: max 256 bytes
	}
	if _, err := rand.Read(unsafe.Slice(buf, n)); err != nil {
		return -int64(syscall.EIO)
	}
	return 0
}

//go:linkname __c2go_getpagesize
func __c2go_getpagesize() int64 { return int64(os.Getpagesize()) }

// sysconf whitelist (#675): _SC_PAGESIZE(30)/_SC_NPROCESSORS_ONLN(84)/
// _SC_CLK_TCK(2) — musl name values, mirrored in <unistd.h>. Everything else
// is -EINVAL.
//go:linkname __c2go_sysconf
func __c2go_sysconf(name int32) int64 {
	switch name {
	case 30:
		return int64(os.Getpagesize())
	case 84:
		return int64(runtime.NumCPU())
	case 2:
		return 100
	}
	return -int64(syscall.EINVAL)
}

// __c2go_opaque_use is explicit_bzero's dead-store barrier (string.c): an
// opaque cross-boundary call the optimizer cannot see through, standing in
// for musl's empty inline-asm memory clobber (c2go has no inline asm).
//
//go:linkname __c2go_opaque_use
func __c2go_opaque_use(p unsafe.Pointer) {
	_ = p
}
