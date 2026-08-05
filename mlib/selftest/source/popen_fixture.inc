/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_POPEN
#error "C2GO_MLIB_TEST_POPEN must name the managed popen"
#endif
#ifndef C2GO_MLIB_TEST_PCLOSE
#error "C2GO_MLIB_TEST_PCLOSE must name the managed pclose"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the exported popen selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#define C2GO_MLIB_POPEN_WRITE_COMMAND "more > NUL"
#else
#define C2GO_MLIB_POPEN_WRITE_COMMAND "cat > /dev/null"
#endif

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    C2GO_MLIB_TEST_FILE *stream;
    char output[64] = {0};
    int status;

    errno = 0;
    if (C2GO_MLIB_TEST_POPEN("echo managed-pipe", "x") || errno != EINVAL)
        return 1;

    stream = C2GO_MLIB_TEST_POPEN("echo managed-pipe", "r");
    if (!stream) return 2;
    /* The carrier is now the only root for the direct Go process pointer. */
    C2GO_MLIB_TEST_FORCE_GC();
    if (!C2GO_MLIB_TEST_FGETS(output, sizeof(output), stream) ||
        strncmp(output, "managed-pipe", sizeof("managed-pipe") - 1))
        return 3;
    status = C2GO_MLIB_TEST_PCLOSE(stream);
    if (status != 0) return 4;

    stream = C2GO_MLIB_TEST_POPEN(C2GO_MLIB_POPEN_WRITE_COMMAND, "w");
    if (!stream) return 5;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FPUTS("managed-pipe\n", stream) < 0) return 6;
    status = C2GO_MLIB_TEST_PCLOSE(stream);
    if (status != 0) return 7;

    /* Rejecting a non-process stream must leave it open for ordinary close. */
    stream = C2GO_MLIB_TEST_FMEMOPEN((void *)0, 32, "w+");
    if (!stream) return 8;
    errno = 0;
    if (C2GO_MLIB_TEST_PCLOSE(stream) != -1 || errno != EINVAL) return 9;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 10;
    return 0;
}

#undef C2GO_MLIB_POPEN_WRITE_COMMAND
