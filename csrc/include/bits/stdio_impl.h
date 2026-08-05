/* bits/stdio_impl.h — private FILE layout (mirrors musl's struct _IO_FILE so
 * the ported musl stdio sources fit unchanged). This is the definition behind
 * the opaque `FILE` in <bits/alltypes.h>; only c2go-libc's own stdio .c files
 * include it. FILE + buffer are PLAIN unmanaged memory: every field is a byte,
 * an int, a size, or a function pointer (uintptr in c2go) — no GC-managed
 * pointer — so no gc_malloc / write barriers are involved. Including the
 * c2go-only tail fields below, the current layout is 256 bytes on Unix and
 * 248 bytes on Windows (where long is 32-bit). */
#ifndef _BITS_STDIO_IMPL_H
#define _BITS_STDIO_IMPL_H

#include <stdio.h>   /* FILE, size_t, off_t */

#define UNGET 8

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF  16
#define F_ERR  32
#define F_SVB  64
#define F_APP  128

struct _c2go_FILE {
	unsigned flags;
	unsigned char *rpos, *rend;
	int (*close)(FILE *);
	unsigned char *wend, *wpos;
	unsigned char *mustbezero_1;
	unsigned char *wbase;
	size_t (*read)(FILE *, unsigned char *, size_t);
	size_t (*write)(FILE *, const unsigned char *, size_t);
	off_t (*seek)(FILE *, off_t, int);
	unsigned char *buf;
	size_t buf_size;
	FILE *prev, *next;
	int fd;
	int pipe_pid;
	long lockcount;
	int mode;
	volatile int lock;
	int lbf;
	void *cookie;
	off_t off;
	char *getln_buf;
	void *mustbezero_2;
	unsigned char *shend;
	off_t shlim, shcnt;
	FILE *prev_locked, *next_locked;
	void *locale;   /* single "C" locale; kept for layout parity, always null */
	/* NOT in musl's _IO_FILE — appended at the END so the mirrored offsets above
	 * are undisturbed. UTF-16 (Windows) wide char I/O only: a pending surrogate
	 * half — the HIGH buffered by fputwc awaiting its LOW, or the LOW buffered by
	 * fgetwc to return on the next call. 0 = none. Unused on the UTF-32 targets. */
	unsigned wpend;
	/* Per-FILE lock (#659, decision ②): a generation-stamped handle into the
	 * Go-side fileLockTab, lazily created on first FLOCK. 0 = not yet created;
	 * streams with lock < 0 (stack FILEs) never engage it. Replaces the old
	 * process-global stdio mutex, whose one blocking read froze every stream. */
	unsigned long long lockid;
	/* popen handle (#662): the Go-side popenTab id pclose Wait()s on. 0 = not
	 * a popen stream. (musl reuses pipe_pid; a generation id needs 64 bits.) */
	unsigned long long pipe_id;
};

#endif /* _BITS_STDIO_IMPL_H */
