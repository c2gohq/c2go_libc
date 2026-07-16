/* intl.c — the GNU gettext compatibility layer (musl src/locale/textdomain.c,
 * dcngettext.c, bind_textdomain_codeset.c). c2go-libc ships no message
 * catalogs and only the C.UTF-8 locale exists, so musl dcngettext's catalog
 * branch (loc->cat[category] — never populated here) is unreachable: every
 * lookup ends at musl's `notrans` label — select msgid1/msgid2 by n, leave
 * errno alone. textdomain and bindtextdomain keep real, queryable state in
 * musl's shape. catopen/catgets are deliberately not provided (#652). */
#include <libintl.h>
#include <locale.h>   /* LC_MESSAGES */
#include <string.h>
#include <strings.h>  /* strcasecmp for bind_textdomain_codeset */
#include <stdlib.h>
#include <errno.h>
#include <limits.h>   /* NAME_MAX / PATH_MAX */
#include <pthread.h>
#include <c2go.h>

static char *current_domain;

static char *__gettextdomain(void)
{
	return current_domain ? current_domain : (char *)"messages";
}

c2go_extern char *textdomain(const char *domainname)
{
	if (!domainname) return __gettextdomain();

	size_t domlen = strlen(domainname);
	if (domlen > NAME_MAX) {
		errno = EINVAL;
		return 0;
	}

	if (!current_domain) {
		current_domain = malloc(NAME_MAX+1);
		if (!current_domain) return 0;
	}

	memcpy(current_domain, domainname, domlen+1);

	return current_domain;
}

c2go_extern char *gettext(const char *msgid)
{
	return dgettext(0, msgid);
}

c2go_extern char *ngettext(const char *msgid1, const char *msgid2, unsigned long n)
{
	return dngettext(0, msgid1, msgid2, n);
}

/* ── bindtextdomain (musl dcngettext.c): a prepend-only binding list ────────
 * musl's readers walk the list locklessly via atomics; here readers and
 * writers share the mutex — simpler, and nodes are still never freed (a
 * returned dirname pointer stays valid for the process lifetime, as callers
 * assume). The nodes live in malloc storage (handle-table rooted). */

struct binding {
	struct binding *next;
	int active;
	char *domainname;
	char *dirname;
	char buf[];
};

static struct binding *bindings;
static pthread_mutex_t intl_lock = PTHREAD_MUTEX_INITIALIZER;

c2go_extern char *bindtextdomain(const char *domainname, const char *dirname)
{
	struct binding *p, *q;

	if (!domainname) return 0;
	if (!dirname) {
		char *ret = 0;
		pthread_mutex_lock(&intl_lock);
		for (p=bindings; p; p=p->next) {
			if (!strcmp(p->domainname, domainname) && p->active) {
				ret = p->dirname;
				break;
			}
		}
		pthread_mutex_unlock(&intl_lock);
		return ret;
	}

	size_t domlen = strnlen(domainname, NAME_MAX+1);
	size_t dirlen = strnlen(dirname, PATH_MAX);
	if (domlen > NAME_MAX || dirlen >= PATH_MAX) {
		errno = EINVAL;
		return 0;
	}

	pthread_mutex_lock(&intl_lock);

	for (p=bindings; p; p=p->next) {
		if (!strcmp(p->domainname, domainname) &&
		    !strcmp(p->dirname, dirname)) {
			break;
		}
	}

	if (!p) {
		p = calloc(sizeof *p + domlen + dirlen + 2, 1);
		if (!p) {
			pthread_mutex_unlock(&intl_lock);
			return 0;
		}
		p->next = bindings;
		p->domainname = p->buf;
		p->dirname = p->buf + domlen + 1;
		memcpy(p->domainname, domainname, domlen+1);
		memcpy(p->dirname, dirname, dirlen+1);
		bindings = p;
	}

	p->active = 1;
	for (q=bindings; q; q=q->next) {
		if (!strcmp(q->domainname, domainname) && q != p)
			q->active = 0;
	}

	pthread_mutex_unlock(&intl_lock);

	return p->dirname;
}

/* Every musl path funnels here to `notrans` (no binding, no locale map, or a
 * failed catalog load all yield the same result), so the reduction below IS
 * the complete behavior, errno preserved by never touching it. */
c2go_extern char *dcngettext(const char *domainname, const char *msgid1,
                             const char *msgid2, unsigned long n, int category)
{
	(void)domainname;
	(void)category;
	return (char *)(n == 1 ? msgid1 : msgid2);
}

c2go_extern char *dcgettext(const char *domainname, const char *msgid, int category)
{
	return dcngettext(domainname, msgid, 0, 1, category);
}

c2go_extern char *dngettext(const char *domainname, const char *msgid1,
                            const char *msgid2, unsigned long n)
{
	return dcngettext(domainname, msgid1, msgid2, n, LC_MESSAGES);
}

c2go_extern char *dgettext(const char *domainname, const char *msgid)
{
	return dcngettext(domainname, msgid, 0, 1, LC_MESSAGES);
}

c2go_extern char *bind_textdomain_codeset(const char *domainname, const char *codeset)
{
	(void)domainname;
	if (codeset && strcasecmp(codeset, "UTF-8")) {
		errno = EINVAL;
		return 0;
	}
	return (char *)"UTF-8";
}
