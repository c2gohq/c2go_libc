/* __strerror.h — musl src/errno/__strerror.h (#657, verbatim messages).
 * The first entry is a catch-all for codes not enumerated here. This file is
 * included multiple times by string.c to build the message blob and the
 * errno -> offset lookup table (musl's struct/offsetof X-macro scheme).
 *
 * c2go adaptation: every named entry is wrapped in #ifdef, because the three
 * targets' <bits/errno.h> deliberately expose different name sets (MinGW has
 * no EMULTIHOP etc.); an unguarded missing name would be a compile error.
 * Entry ORDER and TEXT are musl's, unmodified. */

E(0,            "No error information")
#ifdef EILSEQ
E(EILSEQ,       "Illegal byte sequence")
#endif
#ifdef EDOM
E(EDOM,         "Domain error")
#endif
#ifdef ERANGE
E(ERANGE,       "Result not representable")
#endif
#ifdef ENOTTY
E(ENOTTY,       "Not a tty")
#endif
#ifdef EACCES
E(EACCES,       "Permission denied")
#endif
#ifdef EPERM
E(EPERM,        "Operation not permitted")
#endif
#ifdef ENOENT
E(ENOENT,       "No such file or directory")
#endif
#ifdef ESRCH
E(ESRCH,        "No such process")
#endif
#ifdef EEXIST
E(EEXIST,       "File exists")
#endif
#ifdef EOVERFLOW
E(EOVERFLOW,    "Value too large for data type")
#endif
#ifdef ENOSPC
E(ENOSPC,       "No space left on device")
#endif
#ifdef ENOMEM
E(ENOMEM,       "Out of memory")
#endif
#ifdef EBUSY
E(EBUSY,        "Resource busy")
#endif
#ifdef EINTR
E(EINTR,        "Interrupted system call")
#endif
#ifdef EAGAIN
E(EAGAIN,       "Resource temporarily unavailable")
#endif
#ifdef ESPIPE
E(ESPIPE,       "Invalid seek")
#endif
#ifdef EXDEV
E(EXDEV,        "Cross-device link")
#endif
#ifdef EROFS
E(EROFS,        "Read-only file system")
#endif
#ifdef ENOTEMPTY
E(ENOTEMPTY,    "Directory not empty")
#endif
#ifdef ECONNRESET
E(ECONNRESET,   "Connection reset by peer")
#endif
#ifdef ETIMEDOUT
E(ETIMEDOUT,    "Operation timed out")
#endif
#ifdef ECONNREFUSED
E(ECONNREFUSED, "Connection refused")
#endif
#ifdef EHOSTDOWN
E(EHOSTDOWN,    "Host is down")
#endif
#ifdef EHOSTUNREACH
E(EHOSTUNREACH, "Host is unreachable")
#endif
#ifdef EADDRINUSE
E(EADDRINUSE,   "Address in use")
#endif
#ifdef EPIPE
E(EPIPE,        "Broken pipe")
#endif
#ifdef EIO
E(EIO,          "I/O error")
#endif
#ifdef ENXIO
E(ENXIO,        "No such device or address")
#endif
#ifdef ENOTBLK
E(ENOTBLK,      "Block device required")
#endif
#ifdef ENODEV
E(ENODEV,       "No such device")
#endif
#ifdef ENOTDIR
E(ENOTDIR,      "Not a directory")
#endif
#ifdef EISDIR
E(EISDIR,       "Is a directory")
#endif
#ifdef ETXTBSY
E(ETXTBSY,      "Text file busy")
#endif
#ifdef ENOEXEC
E(ENOEXEC,      "Exec format error")
#endif
#ifdef EINVAL
E(EINVAL,       "Invalid argument")
#endif
#ifdef E2BIG
E(E2BIG,        "Argument list too long")
#endif
#ifdef ELOOP
E(ELOOP,        "Symbolic link loop")
#endif
#ifdef ENAMETOOLONG
E(ENAMETOOLONG, "Filename too long")
#endif
#ifdef ENFILE
E(ENFILE,       "Too many open files in system")
#endif
#ifdef EMFILE
E(EMFILE,       "No file descriptors available")
#endif
#ifdef EBADF
E(EBADF,        "Bad file descriptor")
#endif
#ifdef ECHILD
E(ECHILD,       "No child process")
#endif
#ifdef EFAULT
E(EFAULT,       "Bad address")
#endif
#ifdef EFBIG
E(EFBIG,        "File too large")
#endif
#ifdef EMLINK
E(EMLINK,       "Too many links")
#endif
#ifdef ENOLCK
E(ENOLCK,       "No locks available")
#endif
#ifdef EDEADLK
E(EDEADLK,      "Resource deadlock would occur")
#endif
#ifdef ENOTRECOVERABLE
E(ENOTRECOVERABLE, "State not recoverable")
#endif
#ifdef EOWNERDEAD
E(EOWNERDEAD,   "Previous owner died")
#endif
#ifdef ECANCELED
E(ECANCELED,    "Operation canceled")
#endif
#ifdef ENOSYS
E(ENOSYS,       "Function not implemented")
#endif
#ifdef ENOMSG
E(ENOMSG,       "No message of desired type")
#endif
#ifdef EIDRM
E(EIDRM,        "Identifier removed")
#endif
#ifdef ENOSTR
E(ENOSTR,       "Device not a stream")
#endif
#ifdef ENODATA
E(ENODATA,      "No data available")
#endif
#ifdef ETIME
E(ETIME,        "Device timeout")
#endif
#ifdef ENOSR
E(ENOSR,        "Out of streams resources")
#endif
#ifdef ENOLINK
E(ENOLINK,      "Link has been severed")
#endif
#ifdef EPROTO
E(EPROTO,       "Protocol error")
#endif
#ifdef EBADMSG
E(EBADMSG,      "Bad message")
#endif
#ifdef EBADFD
E(EBADFD,       "File descriptor in bad state")
#endif
#ifdef ENOTSOCK
E(ENOTSOCK,     "Not a socket")
#endif
#ifdef EDESTADDRREQ
E(EDESTADDRREQ, "Destination address required")
#endif
#ifdef EMSGSIZE
E(EMSGSIZE,     "Message too large")
#endif
#ifdef EPROTOTYPE
E(EPROTOTYPE,   "Protocol wrong type for socket")
#endif
#ifdef ENOPROTOOPT
E(ENOPROTOOPT,  "Protocol not available")
#endif
#ifdef EPROTONOSUPPORT
E(EPROTONOSUPPORT,"Protocol not supported")
#endif
#ifdef ESOCKTNOSUPPORT
E(ESOCKTNOSUPPORT,"Socket type not supported")
#endif
#ifdef ENOTSUP
E(ENOTSUP,      "Not supported")
#endif
#ifdef EPFNOSUPPORT
E(EPFNOSUPPORT, "Protocol family not supported")
#endif
#ifdef EAFNOSUPPORT
E(EAFNOSUPPORT, "Address family not supported by protocol")
#endif
#ifdef EADDRNOTAVAIL
E(EADDRNOTAVAIL,"Address not available")
#endif
#ifdef ENETDOWN
E(ENETDOWN,     "Network is down")
#endif
#ifdef ENETUNREACH
E(ENETUNREACH,  "Network unreachable")
#endif
#ifdef ENETRESET
E(ENETRESET,    "Connection reset by network")
#endif
#ifdef ECONNABORTED
E(ECONNABORTED, "Connection aborted")
#endif
#ifdef ENOBUFS
E(ENOBUFS,      "No buffer space available")
#endif
#ifdef EISCONN
E(EISCONN,      "Socket is connected")
#endif
#ifdef ENOTCONN
E(ENOTCONN,     "Socket not connected")
#endif
#ifdef ESHUTDOWN
E(ESHUTDOWN,    "Cannot send after socket shutdown")
#endif
#ifdef EALREADY
E(EALREADY,     "Operation already in progress")
#endif
#ifdef EINPROGRESS
E(EINPROGRESS,  "Operation in progress")
#endif
#ifdef ESTALE
E(ESTALE,       "Stale file handle")
#endif
#ifdef EUCLEAN
E(EUCLEAN,      "Data consistency error")
#endif
#ifdef ENAVAIL
E(ENAVAIL,      "Resource not available")
#endif
#ifdef EREMOTEIO
E(EREMOTEIO,    "Remote I/O error")
#endif
#ifdef EDQUOT
E(EDQUOT,       "Quota exceeded")
#endif
#ifdef ENOMEDIUM
E(ENOMEDIUM,    "No medium found")
#endif
#ifdef EMEDIUMTYPE
E(EMEDIUMTYPE,  "Wrong medium type")
#endif
#ifdef EMULTIHOP
E(EMULTIHOP,    "Multihop attempted")
#endif
#ifdef ENOKEY
E(ENOKEY,       "Required key not available")
#endif
#ifdef EKEYEXPIRED
E(EKEYEXPIRED,  "Key has expired")
#endif
#ifdef EKEYREVOKED
E(EKEYREVOKED,  "Key has been revoked")
#endif
#ifdef EKEYREJECTED
E(EKEYREJECTED, "Key was rejected by service")
#endif
