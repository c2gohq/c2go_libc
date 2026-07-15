//go:build windows

package libc

// POSIX errno values returned by the pthread/semaphore surface on Windows. The
// numbers MUST match c2go-libc's <bits/errno.h> for the x86_64-pc-windows-goabi
// target (MinGW-w64 values) — NOT Go's windows syscall.Errno, whose POSIX names
// carry APPLICATION_ERROR-offset values (e.g. EBUSY = 1<<29+n, not 16) that a C
// caller comparing against EBUSY would never match. Pinned to the values
// obtained by preprocessing errno.h for that target:
//
//	EPERM=1 EAGAIN=11 EBUSY=16 EINVAL=22 ETIMEDOUT=138 ENOTSUP=129 EDEADLK=36 ESRCH=3
const (
	errEPERM     int32 = 1
	errEAGAIN    int32 = 11
	errEBUSY     int32 = 16
	errEINVAL    int32 = 22
	errEFAULT    int32 = 14
	errETIMEDOUT int32 = 138
	errEDEADLK   int32 = 36
	errESRCH     int32 = 3
)

// File-system errno values (same MinGW-w64 <bits/errno.h> source), used by
// winErrno() in io_windows.go to translate the raw Windows system error codes
// that Go's DeleteFile/RemoveDirectory/MoveFileEx return into the CRT-native
// errno a ported C caller expects — reproducing msvcrt's _dosmaperr table so the
// Go-backed path ops report the identical errno the msvcrt fd layer would.
const (
	errENOENT    int32 = 2
	errE2BIG     int32 = 7
	errENOEXEC   int32 = 8
	errEBADF     int32 = 9
	errECHILD    int32 = 10
	errENOMEM    int32 = 12
	errEACCES    int32 = 13
	errEEXIST    int32 = 17
	errEXDEV     int32 = 18
	errEMFILE    int32 = 24
	errENOSPC    int32 = 28
	errEPIPE     int32 = 32
	errENOTEMPTY int32 = 41
	errEILSEQ    int32 = 42 // iconv: invalid/unmappable sequence (MinGW-w64)

	// #647 Windows fsops/stat port (MinGW-native values, bits/errno.h):
	errERANGE       int32 = 34 // getcwd buffer too small
	errENAMETOOLONG int32 = 38 // realpath result exceeds the caller's buffer
)
