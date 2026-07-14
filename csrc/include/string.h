/* string.h — memory and string operations.
 *
 * memcpy/memmove/memset/bzero are compiler-intercepted: clang folds them to
 * @llvm.memcpy/@llvm.memset and the c2go-memcpy-typing pass routes them to
 * runtime.memmove / runtime.typedmemmove (write barriers). They are declared
 * PLAIN here (no c2go_linkname), so that intrinsic folding and LLVM's TLI
 * substitutions keep working; c2go-libc supplies raw-name ABI0 fallbacks.
 *
 * The other functions are C-implemented (source/string.c). Each declaration
 * carries c2go_linkname naming the Go symbol it is reached as (and implying the
 * GoABI0 boundary calling convention); the matching definition in string.c is
 * marked c2go_extern, which owns the exported symbol + the .go binding. */
#ifndef _STRING_H
#define _STRING_H

#define __NEED_size_t
#define __NEED_locale_t
#include <bits/alltypes.h>
#include <c2go.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* compiler-intercepted (plain, NOT linknamed) */
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
void  bzero(void *, size_t);

int memcmp(const void *, const void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.memcmp", C2GO_GOABI0);
void *memchr(const void *, int, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.memchr", C2GO_GOABI0);
void *memrchr(const void *, int, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.memrchr", C2GO_GOABI0);

size_t strlen(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strlen", C2GO_GOABI0);
size_t strnlen(const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strnlen", C2GO_GOABI0);
int strcmp(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcmp", C2GO_GOABI0);
int strncmp(const char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strncmp", C2GO_GOABI0);
char *strcpy(char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcpy", C2GO_GOABI0);
char *stpcpy(char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.stpcpy", C2GO_GOABI0);
char *strncpy(char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strncpy", C2GO_GOABI0);
char *stpncpy(char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.stpncpy", C2GO_GOABI0);
size_t strlcpy(char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strlcpy", C2GO_GOABI0);
size_t strlcat(char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strlcat", C2GO_GOABI0);
char *strcat(char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcat", C2GO_GOABI0);
char *strncat(char *, const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strncat", C2GO_GOABI0);
char *strchr(const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strchr", C2GO_GOABI0);
char *strchrnul(const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strchrnul", C2GO_GOABI0);
char *strrchr(const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strrchr", C2GO_GOABI0);
size_t strspn(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strspn", C2GO_GOABI0);
size_t strcspn(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcspn", C2GO_GOABI0);
char *strpbrk(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strpbrk", C2GO_GOABI0);
char *strstr(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strstr", C2GO_GOABI0);
char *strcasestr(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcasestr", C2GO_GOABI0);

char *strdup(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strdup", C2GO_GOABI0);
char *strndup(const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strndup", C2GO_GOABI0);
char *strtok(char *__restrict, const char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtok", C2GO_GOABI0);
char *strtok_r(char *__restrict, const char *__restrict, char **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtok_r", C2GO_GOABI0);
char *strsep(char **__restrict, const char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strsep", C2GO_GOABI0);
void *memccpy(void *__restrict, const void *__restrict, int, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.memccpy", C2GO_GOABI0);
void *mempcpy(void *__restrict, const void *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mempcpy", C2GO_GOABI0);
void *memmem(const void *, size_t, const void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.memmem", C2GO_GOABI0);
char *strerror(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strerror", C2GO_GOABI0);
int strerror_r(int, char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strerror_r", C2GO_GOABI0);
/* one locale + no message catalogs, so strerror_l ignores its locale_t (musl's
 * __strerror_l translates via LC_MESSAGES; with no catalogs that is a no-op). */
char *strerror_l(int, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strerror_l", C2GO_GOABI0);

/* locale-aware collation (source/locale.c). c2go has only the C locale, so
 * these collate by code point; the _l variants ignore their locale_t. */
int    strcoll(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcoll", C2GO_GOABI0);
int    strcoll_l(const char *, const char *, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strcoll_l", C2GO_GOABI0);
size_t strxfrm(char *__restrict, const char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strxfrm", C2GO_GOABI0);
size_t strxfrm_l(char *__restrict, const char *__restrict, size_t, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strxfrm_l", C2GO_GOABI0);

/* string long tail (source/string.c). strsignal maps the target's NATIVE
 * signal numbers (musl's compile-time remap handles darwin); unix-only —
 * windows has just the six ANSI signals and MinGW ships no strsignal. */
#if !defined(_WIN32)
char *strsignal(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strsignal", C2GO_GOABI0);
#endif
int strverscmp(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.strverscmp", C2GO_GOABI0);
/* dead-store-elimination-proof zeroing: the musl asm barrier is an opaque
 * cross-boundary Go call here (c2go has no inline asm). */
void explicit_bzero(void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.explicit_bzero", C2GO_GOABI0);

#endif /* _STRING_H */
