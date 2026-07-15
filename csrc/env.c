/* env.c — the environ global + putenv/clearenv (#675 stage B).
 *
 * getenv/setenv/unsetenv are Go bridges (process.go) over the os package,
 * which OWNS the process environment under the Go runtime (os/exec child
 * spawning and Go-side readers all go through it) — so musl's model of a
 * C-owned __environ array cannot be the source of truth here. environ is a
 * REBUILT SNAPSHOT instead: every mutation routed through this libc
 * (setenv/unsetenv/putenv/clearenv — the Go bridges call __environ_sync
 * after writing) refreshes the array. Programs that only READ environ see a
 * consistent view; the recorded deviations from musl:
 *   - putenv COPIES the string (musl keeps the caller's pointer live in the
 *     array; with os as the truth a reference cannot be honored — POSIX
 *     leaves the retention model unspecified, glibc-only code that later
 *     mutates the putenv'd buffer will not see the change reflected);
 *   - direct writes into environ[] (legacy idiom) are not propagated back.
 * Storage: the array and every string are libc-malloc'd (unmanaged C heap,
 * handle-table rooted); the environ pointer itself is a C global with a
 * pointer, so the #646 cede machinery gives it Go-owned storage + barriers. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <c2go.h>

char **environ;

/* Go bridge: a freshly malloc'd, NULL-terminated char** snapshot of
 * os.Environ() (array and strings both libc-malloc'd). */
char **__c2go_environ_snapshot(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_environ_snapshot", C2GO_GOABI0);
void __c2go_os_clearenv(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_os_clearenv", C2GO_GOABI0);

c2go_extern void __environ_sync(void)
{
	if (environ) {
		for (char **e = environ; *e; e++) free(*e);
		free(environ);
	}
	environ = __c2go_environ_snapshot();
}

c2go_extern int putenv(char *s)
{
	char *eq = strchr(s, '=');
	if (!eq || eq == s) return unsetenv(s);
	size_t l = eq - s;
	char *k = malloc(l + 1);
	if (!k) return -1;
	memcpy(k, s, l);
	k[l] = 0;
	int r = setenv(k, eq + 1, 1); /* the Go bridge; it calls __environ_sync */
	free(k);
	return r;
}

c2go_extern int clearenv(void)
{
	__c2go_os_clearenv();
	__environ_sync();
	return 0;
}
