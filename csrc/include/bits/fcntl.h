#ifndef _BITS_FCNTL_H
#define _BITS_FCNTL_H

/* c2go-libc: per-OS open() flag bits (Linux=musl, macOS=Darwin,
 * Windows=MinGW). The macro name is the portable interface; the value is
 * each OS's native bit. The Go open bridge maps these to the host open path
 * (forcing O_BINARY on Windows). See ../PORTABILITY.md. */

/* Access modes are 0/1/2/3 on all three. (musl folds O_PATH into O_ACCMODE;
 * c2go does not expose O_PATH, so O_ACCMODE is the plain 2-bit mask.) */
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_ACCMODE 3

#if defined(__linux__)
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_NDELAY    O_NONBLOCK
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000

#elif defined(__APPLE__)
#define O_NONBLOCK  0x00000004
#define O_APPEND    0x00000008
#define O_NDELAY    O_NONBLOCK
#define O_NOFOLLOW  0x00000100
#define O_CREAT     0x00000200
#define O_TRUNC     0x00000400
#define O_EXCL      0x00000800
#define O_NOCTTY    0x00020000
#define O_DIRECTORY 0x00100000
#define O_CLOEXEC   0x01000000

#elif defined(_WIN32)
#define O_APPEND    0x0008
#define O_TEMPORARY 0x0040   /* MinGW _O_TEMPORARY: delete the file when its last
                              * fd closes — Windows cannot unlink an open file, so
                              * this is how tmpfile() gets a delete-on-close stream */
#define O_CLOEXEC   0x0080   /* MinGW _O_NOINHERIT: the CRT's close-on-exec bit
                              * (an OPEN flag on Windows, not a post-open fcntl) */
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_EXCL      0x0400
#define O_TEXT      0x4000   /* Windows-only: CRLF text mode */
#define O_BINARY    0x8000   /* Windows-only: no CRLF translation */
/* Windows has no O_NONBLOCK / O_DIRECTORY / O_NOFOLLOW / O_NOCTTY */

#else
#error "unsupported c2go OS"
#endif

#endif /* _BITS_FCNTL_H */
