/* ctype.h — character classification (C locale). All operate on an int that
 * is either an unsigned char value or EOF.
 *
 * C-implemented (source/ctype.c). Each declaration carries c2go_linkname naming
 * the Go symbol it is reached as (and implying the GoABI0 boundary calling
 * convention); the matching definition in ctype.c is marked c2go_extern, which
 * owns the exported symbol + the .go binding. */
#ifndef _CTYPE_H
#define _CTYPE_H

#define __NEED_locale_t
#include <bits/alltypes.h>
#include <c2go.h>

int isalnum(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isalnum", C2GO_GOABI0);
int isalpha(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isalpha", C2GO_GOABI0);
int isblank(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isblank", C2GO_GOABI0);
int iscntrl(int)  c2go_linkname("github.com/c2gohq/c2go_libc.iscntrl", C2GO_GOABI0);
int isdigit(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isdigit", C2GO_GOABI0);
int isgraph(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isgraph", C2GO_GOABI0);
int islower(int)  c2go_linkname("github.com/c2gohq/c2go_libc.islower", C2GO_GOABI0);
int isprint(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isprint", C2GO_GOABI0);
int ispunct(int)  c2go_linkname("github.com/c2gohq/c2go_libc.ispunct", C2GO_GOABI0);
int isspace(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isspace", C2GO_GOABI0);
int isupper(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isupper", C2GO_GOABI0);
int isxdigit(int) c2go_linkname("github.com/c2gohq/c2go_libc.isxdigit", C2GO_GOABI0);
int isascii(int)  c2go_linkname("github.com/c2gohq/c2go_libc.isascii", C2GO_GOABI0);
int tolower(int)  c2go_linkname("github.com/c2gohq/c2go_libc.tolower", C2GO_GOABI0);
int toupper(int)  c2go_linkname("github.com/c2gohq/c2go_libc.toupper", C2GO_GOABI0);
int toascii(int)  c2go_linkname("github.com/c2gohq/c2go_libc.toascii", C2GO_GOABI0);

/* locale variants (source/ctype.c). One locale, so each ignores its locale_t —
 * this is musl's own design (every __isX_l just `return isX(c)`). isascii/toascii
 * have no _l (not locale-dependent; musl provides none either). */
int isalnum_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isalnum_l", C2GO_GOABI0);
int isalpha_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isalpha_l", C2GO_GOABI0);
int isblank_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isblank_l", C2GO_GOABI0);
int iscntrl_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.iscntrl_l", C2GO_GOABI0);
int isdigit_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isdigit_l", C2GO_GOABI0);
int isgraph_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isgraph_l", C2GO_GOABI0);
int islower_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.islower_l", C2GO_GOABI0);
int isprint_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isprint_l", C2GO_GOABI0);
int ispunct_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.ispunct_l", C2GO_GOABI0);
int isspace_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isspace_l", C2GO_GOABI0);
int isupper_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.isupper_l", C2GO_GOABI0);
int isxdigit_l(int, locale_t) c2go_linkname("github.com/c2gohq/c2go_libc.isxdigit_l", C2GO_GOABI0);
int tolower_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.tolower_l", C2GO_GOABI0);
int toupper_l(int, locale_t)  c2go_linkname("github.com/c2gohq/c2go_libc.toupper_l", C2GO_GOABI0);

#endif /* _CTYPE_H */
