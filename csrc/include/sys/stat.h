/* sys/stat.h — file status. `struct stat` is a UNIFORM c2go layout that the Go
 * stat bridge (stat.go) fills field-by-field from the host stat (syscall.Stat /
 * Lstat / Fstat) — we never memcpy a per-OS struct stat, so there is no per-OS
 * layout and none of Darwin's __DARWIN_UNIX03 / 64-bit-inode variance. The mode
 * bits are likewise uniform (they match natively on Linux/macOS and are simply
 * defined here on Windows too). */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#define __NEED_dev_t
#define __NEED_ino_t
#define __NEED_nlink_t
#define __NEED_mode_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_off_t
#define __NEED_blksize_t
#define __NEED_blkcnt_t
#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>
#include <c2go.h>

struct stat {
	dev_t     st_dev;
	ino_t     st_ino;
	nlink_t   st_nlink;
	mode_t    st_mode;
	uid_t     st_uid;
	gid_t     st_gid;
	dev_t     st_rdev;
	off_t     st_size;
	blksize_t st_blksize;
	blkcnt_t  st_blocks;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
};
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
#define S_IRWXU  0000700
#define S_IRUSR  0000400
#define S_IWUSR  0000200
#define S_IXUSR  0000100
#define S_IRWXG  0000070
#define S_IRGRP  0000040
#define S_IWGRP  0000020
#define S_IXGRP  0000010
#define S_IRWXO  0000007
#define S_IROTH  0000004
#define S_IWOTH  0000002
#define S_IXOTH  0000001

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

int    stat(const char *__restrict, struct stat *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.stat", C2GO_GOABI0);
int    fstat(int, struct stat *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fstat", C2GO_GOABI0);
int    lstat(const char *__restrict, struct stat *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.lstat", C2GO_GOABI0);
int    chmod(const char *, mode_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.chmod", C2GO_GOABI0);
/* fchmod is Unix-only: MinGW-w64 has none (no CRT fd → chmod path), and a
 * fake stub would lie (#647). */
#if !defined(_WIN32)
int    fchmod(int, mode_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.fchmod", C2GO_GOABI0);
#endif
int    mkdir(const char *, mode_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mkdir", C2GO_GOABI0);
#if !defined(_WIN32)
int    mkdirat(int, const char *, mode_t)  /* #675, source/unistd2.c */
    c2go_linkname("github.com/c2gohq/c2go_libc.mkdirat", C2GO_GOABI0);
/* completes the *at family (#675 C wave 2b): same uniform struct stat fill
 * as stat/fstat, AT_* flags in <fcntl.h> (AT_SYMLINK_NOFOLLOW honored). */
int    fstatat(int, const char *__restrict, struct stat *__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fstatat", C2GO_GOABI0);

/* time/stat batch (source/stat2.c). The UTIME_NOW/UTIME_OMIT sentinels are
 * the target's NATIVE values (they travel to the kernel unmapped): linux =
 * musl 0x3fffffff/0x3ffffffe, darwin = xnu -1/-2. */
#if defined(__APPLE__)
#define UTIME_NOW  (-1)
#define UTIME_OMIT (-2)
#else
#define UTIME_NOW  0x3fffffff
#define UTIME_OMIT 0x3ffffffe
#endif
int    utimensat(int, const char *, const struct timespec [2], int)
    c2go_linkname("github.com/c2gohq/c2go_libc.utimensat", C2GO_GOABI0);
int    futimens(int, const struct timespec [2])
    c2go_linkname("github.com/c2gohq/c2go_libc.futimens", C2GO_GOABI0);
int    mkfifo(const char *, mode_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mkfifo", C2GO_GOABI0);
#endif
mode_t umask(mode_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.umask", C2GO_GOABI0);

#endif /* _SYS_STAT_H */
