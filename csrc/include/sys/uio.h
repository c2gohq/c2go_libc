/* sys/uio.h — vectored I/O (musl include/sys/uio.h). struct iovec matches the
 * kernel layout on both unix targets, so the C pointer passes straight to the
 * readv/writev syscall shim. Impl source/unistd2.c (#675). Unix-only. */
#ifndef _SYS_UIO_H
#define _SYS_UIO_H

#include <c2go.h>

#define __NEED_size_t
#define __NEED_ssize_t
#include <bits/alltypes.h>

struct iovec {
	void *iov_base;
	size_t iov_len;
};

#if !defined(_WIN32)
ssize_t readv(int, const struct iovec *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.readv", C2GO_GOABI0);
ssize_t writev(int, const struct iovec *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.writev", C2GO_GOABI0);
#endif

#endif /* _SYS_UIO_H */
