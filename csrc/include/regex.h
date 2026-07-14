/* regex.h — POSIX regular expressions (musl include/regex.h, TRE engine).
 * Impl source/regcomp.c + regexec.c + regerror.c + tre-mem.c (#667). */
#ifndef _REGEX_H
#define _REGEX_H

#include <c2go.h>

#define __NEED_regoff_t
#define __NEED_size_t

#include <bits/alltypes.h>

typedef struct re_pattern_buffer {
	size_t re_nsub;
	void *__opaque, *__padding[4];
	size_t __nsub2;
	char __padding2;
} regex_t;

typedef struct {
	regoff_t rm_so;
	regoff_t rm_eo;
} regmatch_t;

#define REG_EXTENDED    1
#define REG_ICASE       2
#define REG_NEWLINE     4
#define REG_NOSUB       8

#define REG_NOTBOL      1
#define REG_NOTEOL      2

#define REG_OK          0
#define REG_NOMATCH     1
#define REG_BADPAT      2
#define REG_ECOLLATE    3
#define REG_ECTYPE      4
#define REG_EESCAPE     5
#define REG_ESUBREG     6
#define REG_EBRACK      7
#define REG_EPAREN      8
#define REG_EBRACE      9
#define REG_BADBR       10
#define REG_ERANGE      11
#define REG_ESPACE      12
#define REG_BADRPT      13

#define REG_ENOSYS      -1

int regcomp(regex_t *__restrict, const char *__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.regcomp", C2GO_GOABI0);
int regexec(const regex_t *__restrict, const char *__restrict, size_t, regmatch_t *__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.regexec", C2GO_GOABI0);
void regfree(regex_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.regfree", C2GO_GOABI0);

size_t regerror(int, const regex_t *__restrict, char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.regerror", C2GO_GOABI0);

#endif /* _REGEX_H */
