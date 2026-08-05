/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_GLOB_H
#define C2GO_MLIB_GLOB_H

#include <c2go.h>
#include <c2go/mlib/names.h>
#include <stdint.h>

/* Replacement mode imports the standard flags and result codes but supplies a
 * managed glob_t and managed functions. Namespaced mode exposes both worlds. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _GLOB_H
#error "c2go mlib glob replacement must be included before <glob.h>"
#endif
#define C2GO_GLOB_OMIT_TYPE 1
#define C2GO_GLOB_OMIT_FUNCTIONS 1
#include <glob.h>
#undef C2GO_GLOB_OMIT_FUNCTIONS
#undef C2GO_GLOB_OMIT_TYPE
#else
#include <glob.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* gl_pathv is a direct pointer to a typed Go-heap pointer array. The reserved
 * words are integers here because mlib never uses them and the GC must not scan
 * arbitrary caller data as pointers. Layout remains ABI-identical to glob_t. */
typedef struct {
    size_t gl_pathc;
    char **managed gl_pathv;
    size_t gl_offs;
    int __dummy1;
    uintptr_t __dummy2[5];
} C2GO_MLIB_NAME(glob_t);

int C2GO_MLIB_NAME(glob)(const char *__restrict, int,
    int (*)(const char *, int), C2GO_MLIB_NAME(glob_t) *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_glob", C2GO_GOABI0);
void C2GO_MLIB_NAME(globfree)(C2GO_MLIB_NAME(glob_t) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_globfree", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_GLOB_H */
