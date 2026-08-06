/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed directory-stream carrier. This source is instantiated once with the
 * mlib_ namespace. The replacement header maps ordinary names to these same
 * package symbols, so there is no second copy of the implementation. */

#include <c2go/mlib/dirent.h>
#include <errno.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef void *managed mlib_dir_state;
typedef struct dirent *managed mlib_dirent_pointer;

/* Go-owned stream operations (mlib/dirent.go). Errors are returned as positive
 * errno values by open and negative errno values by the other bridge calls. */
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirOpen", C2GO_GOABI0)
int __c2go_mlib_dir_open(const char *path, mlib_dir_state *state);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirRead", C2GO_GOABI0)
int __c2go_mlib_dir_read(mlib_dir_state state, void *entry);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirClose", C2GO_GOABI0)
int __c2go_mlib_dir_close(mlib_dir_state state);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirRewind", C2GO_GOABI0)
int __c2go_mlib_dir_rewind(mlib_dir_state state);
#if !defined(_WIN32)
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirFD", C2GO_GOABI0)
long long __c2go_mlib_dir_fd(mlib_dir_state state);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.DirOpenFD", C2GO_GOABI0)
int __c2go_mlib_dir_open_fd(int fd, mlib_dir_state *state);
#endif

c2go_extern mlib_DIR *mlib_opendir(const char *name)
{
    mlib_dir_state state = (void *)0;
    int e = __c2go_mlib_dir_open(name, &state);
    if (e != 0) {
        errno = e;
        return (void *)0;
    }

    mlib_DIR *dir = gc_malloc(c2go_typeinfo(mlib_DIR), sizeof(*dir));
    if (!dir) {
        (void)__c2go_mlib_dir_close(state);
        state = (mlib_dir_state)0;
        errno = ENOMEM;
        return (void *)0;
    }
    dir->_state = state;
    return dir;
}

c2go_extern struct dirent *mlib_readdir(mlib_DIR *dir)
{
    mlib_dirent_pointer entry = (mlib_dirent_pointer)dir->_entry;
    int r = __c2go_mlib_dir_read(dir->_state, (void *)entry);
    if (r <= 0) {
        if (r < 0) errno = -r;
        return (void *)0;
    }
    /* POSIX exposes this as a borrowed plain struct dirent *. The owning DIR
     * remains the GC root for the embedded buffer until the next stream
     * operation; managed containers such as scandir restore AS1 explicitly. */
    return (struct dirent *)entry;
}

c2go_extern int mlib_readdir_r(mlib_DIR *restrict dir,
                               struct dirent *restrict entry,
                               struct dirent **restrict result)
{
    int r = __c2go_mlib_dir_read(dir->_state, entry);
    if (r < 0) {
        *result = (void *)0;
        return -r;
    }
    *result = r ? entry : (void *)0;
    return 0;
}

c2go_extern int mlib_closedir(mlib_DIR *dir)
{
    /* Keep the direct state pointer in a managed local across the closing Go
     * call, but retire it from the carrier immediately so stale reuse cannot
     * reach a closed stream. There is intentionally no free(): DIR itself is a
     * typed Go-heap object and becomes collectible when callers drop it. */
    mlib_dir_state state = dir->_state;
    dir->_state = (mlib_dir_state)0;
    int r = __c2go_mlib_dir_close(state);
    state = (mlib_dir_state)0;
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return 0;
}

c2go_extern void mlib_rewinddir(mlib_DIR *dir)
{
    int r = __c2go_mlib_dir_rewind(dir->_state);
    if (r < 0) errno = -r;
}

#if !defined(_WIN32)
c2go_extern int mlib_dirfd(mlib_DIR *dir)
{
    long long r = __c2go_mlib_dir_fd(dir->_state);
    if (r < 0) {
        errno = (int)-r;
        return -1;
    }
    return (int)r;
}

c2go_extern mlib_DIR *mlib_fdopendir(int fd)
{
    mlib_dir_state state = (void *)0;
    int e = __c2go_mlib_dir_open_fd(fd, &state);
    if (e != 0) {
        errno = e;
        return (void *)0;
    }

    mlib_DIR *dir = gc_malloc(c2go_typeinfo(mlib_DIR), sizeof(*dir));
    if (!dir) {
        (void)__c2go_mlib_dir_close(state);
        state = (mlib_dir_state)0;
        errno = ENOMEM;
        return (void *)0;
    }
    dir->_state = state;
    return dir;
}
#endif

#pragma c2go pop
