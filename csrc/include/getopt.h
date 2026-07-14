/* getopt.h — GNU getopt_long / getopt_long_only + struct option. Plain getopt and
 * the optarg/optind/opterr/optopt globals come from <unistd.h>. Impl source/getopt.c. */
#ifndef _GETOPT_H
#define _GETOPT_H

#include <unistd.h>   /* getopt + optarg/optind/opterr/optopt */
#include <c2go.h>

extern int optreset;   /* BSD getopt reset flag (musl's __optreset) */

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

int getopt_long(int, char *const *, const char *, const struct option *, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getopt_long", C2GO_GOABI0);
int getopt_long_only(int, char *const *, const char *, const struct option *, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.getopt_long_only", C2GO_GOABI0);

#define no_argument        0
#define required_argument  1
#define optional_argument  2

#endif /* _GETOPT_H */
