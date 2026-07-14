/* libintl.h — the GNU gettext compatibility layer (source/intl.c, musl
 * src/locale/{textdomain,dcngettext,bind_textdomain_codeset}.c). c2go-libc
 * ships no message catalogs and its only locale is C.UTF-8, so every lookup
 * faithfully takes musl's `notrans` path: plural selection between the
 * caller's own msgids, errno untouched. textdomain/bindtextdomain still keep
 * real, queryable state (gettext apps read them back). catopen/catgets — the
 * X/Open message-catalog API proper — are deliberately not provided (#652). */
#ifndef _LIBINTL_H
#define _LIBINTL_H

#include <c2go.h>

char *gettext(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.gettext", C2GO_GOABI0);
char *dgettext(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.dgettext", C2GO_GOABI0);
char *dcgettext(const char *, const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.dcgettext", C2GO_GOABI0);
char *ngettext(const char *, const char *, unsigned long)
    c2go_linkname("github.com/c2gohq/c2go_libc.ngettext", C2GO_GOABI0);
char *dngettext(const char *, const char *, const char *, unsigned long)
    c2go_linkname("github.com/c2gohq/c2go_libc.dngettext", C2GO_GOABI0);
char *dcngettext(const char *, const char *, const char *, unsigned long, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.dcngettext", C2GO_GOABI0);
char *textdomain(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.textdomain", C2GO_GOABI0);
char *bindtextdomain(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.bindtextdomain", C2GO_GOABI0);
char *bind_textdomain_codeset(const char *, const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.bind_textdomain_codeset", C2GO_GOABI0);

#endif /* _LIBINTL_H */
