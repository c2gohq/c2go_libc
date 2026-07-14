/* sys/types.h — POSIX type names (all pulled from the shared type pool). */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_ptrdiff_t
#define __NEED_off_t
#define __NEED_mode_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_dev_t
#define __NEED_ino_t
#define __NEED_nlink_t
#define __NEED_blksize_t
#define __NEED_blkcnt_t
#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_suseconds_t
#define __NEED_intptr_t
#include <bits/alltypes.h>

/* BSD-style aliases (some code expects these). */
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
typedef unsigned short ushort;
typedef unsigned int   uint;

#endif /* _SYS_TYPES_H */
