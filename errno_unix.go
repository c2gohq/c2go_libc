//go:build unix

package libc

import "syscall"

// POSIX errno values returned by the pthread/semaphore surface. On unix the
// c2go-libc <bits/errno.h> uses the platform-native values — exactly what
// syscall.Errno carries — so sourcing them from there resolves per-OS
// differences correctly (e.g. EAGAIN is 35 on darwin, 11 on linux; ETIMEDOUT is
// 60 vs 110). The Windows counterparts are pinned literals in errno_windows.go.
const (
	errEPERM     = int32(syscall.EPERM)
	errEAGAIN    = int32(syscall.EAGAIN)
	errEBUSY     = int32(syscall.EBUSY)
	errEINVAL    = int32(syscall.EINVAL)
	errETIMEDOUT = int32(syscall.ETIMEDOUT)
	errEDEADLK   = int32(syscall.EDEADLK)
	errESRCH     = int32(syscall.ESRCH)
	errE2BIG     = int32(syscall.E2BIG)  // iconv: output buffer full
	errEILSEQ    = int32(syscall.EILSEQ) // iconv: invalid/unmappable sequence
	errEBADF     = int32(syscall.EBADF)  // iconv_close(nil) (#648)
	errENOMEM    = int32(syscall.ENOMEM) // malloc beyond the runtime's limits (#651)
	errEOVERFLOW = int32(syscall.EOVERFLOW)
)
