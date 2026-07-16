/* scandir_selftest.c — in-C exercise of scandir + alphasort / versionsort,
 * driven from Go via ScandirSelftest(). scandir's sel/cmp are C callbacks
 * invoked from C (cmp through qsort), so — like a qsort comparator — they need
 * a C driver; alphasort/versionsort are passed as cmp. Returns 0 on success, a
 * distinct code per failing step. */
#if !defined(_WIN32)

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <c2go.h>

/* a C sel filter: keep only entries whose name ends in ".dat" */
static int only_dat(const struct dirent *d)
{
	size_t n = strlen(d->d_name);
	return n >= 4 && !strcmp(d->d_name + n - 4, ".dat");
}

/* Local (internal-ABI) comparators wrapping the boundary alphasort/versionsort
 * — the c2go idiom for using a libc comparator with scandir (a boundary fp
 * cannot be passed straight as cmp; see dirent.h). These exercise the real
 * alphasort/versionsort inside a caller-owned comparator. */
static int cmp_alpha(const struct dirent **a, const struct dirent **b)
{
	return alphasort(a, b);
}
static int cmp_ver(const struct dirent **a, const struct dirent **b)
{
	return versionsort(a, b);
}

static int index_of(struct dirent **v, int n, const char *name)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(v[i]->d_name, name)) return i;
	return -1;
}

static void free_names(struct dirent **v, int n)
{
	for (int i = 0; i < n; i++) free(v[i]);
	free(v);
}

c2go_extern int ScandirSelftest(void)
{
	const char *tmp = getenv("TMPDIR");
	if (!tmp || !tmp[0]) tmp = "/tmp";
	char dir[600];
	snprintf(dir, sizeof dir, "%s/c2go_scandir_XXXXXX", tmp);
	if (!mkdtemp(dir)) return 1;

	const char *files[] = {"b.dat", "a.dat", "c.txt", "a10.dat", "a9.dat"};
	for (int i = 0; i < 5; i++) {
		char p[700];
		snprintf(p, sizeof p, "%s/%s", dir, files[i]);
		int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) return 2;
		if (close(fd)) return 3;
	}

	/* 1. scandir + alphasort, no filter. POSIX readdir (the c2go bridge too)
	 * yields . and .. first, so 5 files + . + .. = 7 entries. In ASCII order
	 * (C-locale strcoll) the 5 real files sort a.dat < a10.dat < a9.dat < b.dat
	 * < c.txt ('.'=46 < '1'=49 < '9'=57, so "a.dat" precedes both, and "a10"
	 * precedes "a9" because '1' < '9'). */
	struct dirent **names = 0;
	int n = scandir(dir, &names, 0, cmp_alpha);
	if (n != 7) return 4;
	if (index_of(names, n, ".") < 0 || index_of(names, n, "..") < 0) return 5;
	int ia = index_of(names, n, "a.dat"),  iA = index_of(names, n, "a10.dat"),
	    i9 = index_of(names, n, "a9.dat"),  ib = index_of(names, n, "b.dat"),
	    ic = index_of(names, n, "c.txt");
	if (ia < 0 || iA < 0 || i9 < 0 || ib < 0 || ic < 0) return 6;
	if (!(ia < iA && iA < i9 && i9 < ib && ib < ic)) return 7;  /* alphasort order */
	free_names(names, n);

	/* 2. versionsort orders the numeric run naturally: a9.dat BEFORE a10.dat
	 * (9 < 10), the discriminating difference from alphasort. */
	names = 0;
	n = scandir(dir, &names, 0, cmp_ver);
	if (n != 7) return 10;
	if (index_of(names, n, "a9.dat") > index_of(names, n, "a10.dat")) return 11;
	free_names(names, n);

	/* 3. sel filter: keep only *.dat -> 4 entries, no c.txt */
	names = 0;
	n = scandir(dir, &names, only_dat, cmp_alpha);
	if (n != 4) return 12;
	if (index_of(names, n, "c.txt") != -1) return 13;
	if (index_of(names, n, "a.dat") < 0 || index_of(names, n, "b.dat") < 0) return 14;
	free_names(names, n);

	/* 4. error path: a nonexistent directory -> -1 / ENOENT */
	names = 0;
	errno = 0;
	if (scandir("/no/such/c2go/scandir/dir", &names, 0, cmp_alpha) != -1 || errno != ENOENT)
		return 15;

	/* teardown */
	for (int i = 0; i < 5; i++) {
		char p[700];
		snprintf(p, sizeof p, "%s/%s", dir, files[i]);
		unlink(p);
	}
	if (rmdir(dir)) return 16;
	return 0;
}

#endif /* !_WIN32 */
