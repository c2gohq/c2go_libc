/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_STDIO_H
#define C2GO_MLIB_STDIO_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Namespaced mode may use ordinary FILE and mlib_FILE side by side. In
 * replacement mode this header owns FILE and intentionally suppresses the
 * ordinary stdio header: exposing root declarations with a managed FILE type
 * would silently route the wrong carrier into root libc. */
#ifdef C2GO_MLIB_UNPREFIXED
#if defined(_STDIO_H) || defined(__DEFINED_FILE)
#error "c2go mlib stdio replacement must be included before headers that define FILE"
#endif
#define C2GO_MLIB_FILE_REPLACEMENT 1
typedef struct _c2go_mlib_FILE FILE;
#define __DEFINED_FILE 1
#define _STDIO_H 1
#define __NEED_size_t
#define __NEED_off_t
#define __NEED_va_list
#include <bits/alltypes.h>
#else
#include <stdio.h>
typedef struct _c2go_mlib_FILE mlib_FILE;
#endif

#ifdef C2GO_MLIB_UNPREFIXED
#ifndef NULL
#define NULL ((void *)0)
#endif
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 1000
#define TMP_MAX 10000
#define L_tmpnam 20

/* Keep FILE-independent stdio utilities available in replacement mode. They
 * use root libc internally but neither accept nor return a FILE carrier and do
 * not create managed ownership graphs. Allocation-returning asprintf and the
 * scanf family are intentionally excluded from this bridge. */
int remove(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.remove", C2GO_GOABI0);
int rename(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rename", C2GO_GOABI0);
int sprintf(char *__restrict, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.sprintf", C2GO_GOABI0);
int snprintf(char *__restrict, size_t, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.snprintf", C2GO_GOABI0);
int vsprintf(char *__restrict, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vsprintf", C2GO_GOABI0);
int vsnprintf(char *__restrict, size_t, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vsnprintf", C2GO_GOABI0);
int dprintf(int, const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc.dprintf", C2GO_GOABI0);
int vdprintf(int, const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc.vdprintf", C2GO_GOABI0);
#endif

typedef long long C2GO_MLIB_NAME(fpos_t);

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* FILE stays opaque. Instances returned here are typed Go-heap objects: their
 * raw musl engine is a no-scan byte region, while the buffer, lock, and list
 * ownership are retained in explicit managed fields. Never pass this carrier
 * to root libc's stdio functions or to free(). */
typedef struct _c2go_mlib_FILE C2GO_MLIB_NAME(FILE);

C2GO_MLIB_NAME(FILE) *C2GO_MLIB_NAME(fopen)(const char *__restrict,
    const char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fopen", C2GO_GOABI0);
C2GO_MLIB_NAME(FILE) *C2GO_MLIB_NAME(fdopen)(int, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fdopen", C2GO_GOABI0);
int C2GO_MLIB_NAME(fclose)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fclose", C2GO_GOABI0);

int C2GO_MLIB_NAME(fflush)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fflush", C2GO_GOABI0);
int C2GO_MLIB_NAME(feof)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_feof", C2GO_GOABI0);
int C2GO_MLIB_NAME(ferror)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ferror", C2GO_GOABI0);
void C2GO_MLIB_NAME(clearerr)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_clearerr", C2GO_GOABI0);

int C2GO_MLIB_NAME(fseek)(C2GO_MLIB_NAME(FILE) *, long, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fseek", C2GO_GOABI0);
long C2GO_MLIB_NAME(ftell)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ftell", C2GO_GOABI0);
void C2GO_MLIB_NAME(rewind)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_rewind", C2GO_GOABI0);
int C2GO_MLIB_NAME(fgetpos)(C2GO_MLIB_NAME(FILE) *__restrict,
    C2GO_MLIB_NAME(fpos_t) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fgetpos", C2GO_GOABI0);
int C2GO_MLIB_NAME(fsetpos)(C2GO_MLIB_NAME(FILE) *,
    const C2GO_MLIB_NAME(fpos_t) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fsetpos", C2GO_GOABI0);
int C2GO_MLIB_NAME(fseeko)(C2GO_MLIB_NAME(FILE) *, off_t, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fseeko", C2GO_GOABI0);
off_t C2GO_MLIB_NAME(ftello)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ftello", C2GO_GOABI0);

size_t C2GO_MLIB_NAME(fread)(void *__restrict, size_t, size_t,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fread", C2GO_GOABI0);
size_t C2GO_MLIB_NAME(fwrite)(const void *__restrict, size_t, size_t,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fwrite", C2GO_GOABI0);
int C2GO_MLIB_NAME(fgetc)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fgetc", C2GO_GOABI0);
int C2GO_MLIB_NAME(getc)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_getc", C2GO_GOABI0);
int C2GO_MLIB_NAME(ungetc)(int, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ungetc", C2GO_GOABI0);
int C2GO_MLIB_NAME(fputc)(int, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fputc", C2GO_GOABI0);
int C2GO_MLIB_NAME(putc)(int, C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_putc", C2GO_GOABI0);
char *C2GO_MLIB_NAME(fgets)(char *__restrict, int,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fgets", C2GO_GOABI0);
int C2GO_MLIB_NAME(fputs)(const char *__restrict,
    C2GO_MLIB_NAME(FILE) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fputs", C2GO_GOABI0);

int C2GO_MLIB_NAME(vfprintf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const char *__restrict, va_list)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_vfprintf", C2GO_GOABI0);
int C2GO_MLIB_NAME(fprintf)(C2GO_MLIB_NAME(FILE) *__restrict,
    const char *__restrict, ...)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fprintf", C2GO_GOABI0);

int C2GO_MLIB_NAME(fileno)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fileno", C2GO_GOABI0);
void C2GO_MLIB_NAME(flockfile)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_flockfile", C2GO_GOABI0);
int C2GO_MLIB_NAME(ftrylockfile)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ftrylockfile", C2GO_GOABI0);
void C2GO_MLIB_NAME(funlockfile)(C2GO_MLIB_NAME(FILE) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_funlockfile", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_STDIO_H */
