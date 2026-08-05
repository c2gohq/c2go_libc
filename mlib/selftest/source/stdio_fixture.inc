/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_FILE
#error "C2GO_MLIB_TEST_FILE must name the managed FILE type"
#endif
#ifndef C2GO_MLIB_TEST_FPOS
#error "C2GO_MLIB_TEST_FPOS must name the managed fpos type"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the exported selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif
#ifndef C2GO_MLIB_TEST_STDOUT
#error "C2GO_MLIB_TEST_STDOUT must name the managed standard output stream"
#endif
#ifndef C2GO_MLIB_TEST_PRINTF
#error "C2GO_MLIB_TEST_PRINTF must name the managed printf"
#endif
#ifndef C2GO_MLIB_TEST_PUTCHAR
#error "C2GO_MLIB_TEST_PUTCHAR must name the managed putchar"
#endif
#ifndef C2GO_MLIB_TEST_PUTS
#error "C2GO_MLIB_TEST_PUTS must name the managed puts"
#endif
#ifndef C2GO_MLIB_TEST_GETCHAR
#error "C2GO_MLIB_TEST_GETCHAR must name the managed getchar"
#endif
#ifndef C2GO_MLIB_TEST_STDOUT_EXPORT
#error "C2GO_MLIB_TEST_STDOUT_EXPORT must name the standard-output selftest"
#endif
#ifndef C2GO_MLIB_TEST_STDIN_EXPORT
#error "C2GO_MLIB_TEST_STDIN_EXPORT must name the standard-input selftest"
#endif

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int c2go_mlib_stdio_path(char *out, size_t capacity,
                                const char *base, const char *relative)
{
    size_t base_length = strlen(base);
    size_t relative_length = strlen(relative);
    if (base_length + relative_length + 2 > capacity) return -1;
    memcpy(out, base, base_length);
    out[base_length] = '/';
    memcpy(out + base_length + 1, relative, relative_length + 1);
    return 0;
}

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    const char *tmp = getenv("TMPDIR");
    const char expected[] = "value=42!";
    char root[512], path[600], input[32] = {0}, formatted[16];
    C2GO_MLIB_TEST_FILE *stream;
    C2GO_MLIB_TEST_FPOS position;
    int fd;

    /* Replacement mode retains FILE-independent root stdio utilities. */
    if (snprintf(formatted, sizeof(formatted), "value=%d", 42) != 8 ||
        strcmp(formatted, "value=42")) return 26;

    if (!tmp || !tmp[0]) tmp = getenv("TEMP");
    if (!tmp || !tmp[0]) tmp = ".";
    if (c2go_mlib_stdio_path(root, sizeof(root), tmp,
                             "c2go_mlib_stdio_XXXXXX")) return 1;
    if (!mkdtemp(root)) return 1;
    if (c2go_mlib_stdio_path(path, sizeof(path), root, "stream.txt")) return 2;

    stream = C2GO_MLIB_TEST_FOPEN(path, "w+");
    if (!stream) return 3;
    if (C2GO_MLIB_TEST_FILENO(stream) < 0) return 4;

    /* The managed direct lock is recursive, so explicit flockfile composes
     * with wrappers that acquire the same lock internally. */
    C2GO_MLIB_TEST_FLOCKFILE(stream);
    if (C2GO_MLIB_TEST_FPRINTF(stream, "value=%d", 42) != 8) return 5;
    if (C2GO_MLIB_TEST_FPUTC('!', stream) != '!') return 6;
    C2GO_MLIB_TEST_FUNLOCKFILE(stream);

    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FFLUSH((void *)0) != 0) return 7;
    if (C2GO_MLIB_TEST_FTELL(stream) != (long)(sizeof(expected) - 1)) return 8;
    if (C2GO_MLIB_TEST_FGETPOS(stream, &position) != 0) return 9;
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 10;
    if (C2GO_MLIB_TEST_FREAD(input, 1, sizeof(expected) - 1, stream)
            != sizeof(expected) - 1) return 11;
    if (strcmp(input, expected)) return 12;
    if (C2GO_MLIB_TEST_FGETC(stream) != EOF) return 13;
    if (!C2GO_MLIB_TEST_FEOF(stream) || C2GO_MLIB_TEST_FERROR(stream)) return 14;
    C2GO_MLIB_TEST_CLEARERR(stream);
    if (C2GO_MLIB_TEST_FEOF(stream) || C2GO_MLIB_TEST_FERROR(stream)) return 15;
    if (C2GO_MLIB_TEST_FSETPOS(stream, &position) != 0) return 16;
    if (C2GO_MLIB_TEST_FPUTS("x", stream) < 0) return 17;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 18;

    fd = open(path, O_RDONLY);
    if (fd < 0) return 19;
    stream = C2GO_MLIB_TEST_FDOPEN(fd, "r");
    if (!stream) {
        close(fd);
        return 20;
    }
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_GETC(stream) != 'v') return 21;
    if (C2GO_MLIB_TEST_UNGETC('V', stream) != 'V') return 22;
    if (C2GO_MLIB_TEST_GETC(stream) != 'V') return 23;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 24;

    if (unlink(path) || rmdir(root)) return 25;
    return 0;
}

/* Deliberately leave stdout buffered. The Go test calls C2goFinalize and
 * verifies that mlib's internal finalize hook flushes this separate FILE
 * world. The forced collection also proves the global standard-stream slot is
 * emitted as a GC root. */
c2go_extern int C2GO_MLIB_TEST_STDOUT_EXPORT(void)
{
    if (C2GO_MLIB_TEST_PRINTF("managed-stdio:%d", 42) != 16) return 1;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_PUTS("|line") != 0) return 2;
    /* The pipe probe above makes stdout fully buffered; this final byte is the
     * part that must be emitted by the finalize hook. */
    if (C2GO_MLIB_TEST_PUTCHAR('!') != '!') return 3;
    if (!C2GO_MLIB_TEST_STDOUT) return 4;
    return 0;
}

c2go_extern int C2GO_MLIB_TEST_STDIN_EXPORT(void)
{
    return C2GO_MLIB_TEST_GETCHAR();
}

#undef C2GO_MLIB_TEST_STDIN_EXPORT
#undef C2GO_MLIB_TEST_STDOUT_EXPORT
#undef C2GO_MLIB_TEST_GETCHAR
#undef C2GO_MLIB_TEST_PUTS
#undef C2GO_MLIB_TEST_PUTCHAR
#undef C2GO_MLIB_TEST_PRINTF
#undef C2GO_MLIB_TEST_STDOUT
#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_FUNLOCKFILE
#undef C2GO_MLIB_TEST_FLOCKFILE
#undef C2GO_MLIB_TEST_FPRINTF
#undef C2GO_MLIB_TEST_FILENO
#undef C2GO_MLIB_TEST_CLEARERR
#undef C2GO_MLIB_TEST_FERROR
#undef C2GO_MLIB_TEST_FEOF
#undef C2GO_MLIB_TEST_UNGETC
#undef C2GO_MLIB_TEST_GETC
#undef C2GO_MLIB_TEST_FGETC
#undef C2GO_MLIB_TEST_FPUTC
#undef C2GO_MLIB_TEST_FPUTS
#undef C2GO_MLIB_TEST_FREAD
#undef C2GO_MLIB_TEST_FSETPOS
#undef C2GO_MLIB_TEST_FGETPOS
#undef C2GO_MLIB_TEST_FSEEK
#undef C2GO_MLIB_TEST_FTELL
#undef C2GO_MLIB_TEST_FFLUSH
#undef C2GO_MLIB_TEST_FCLOSE
#undef C2GO_MLIB_TEST_FDOPEN
#undef C2GO_MLIB_TEST_FOPEN
#undef C2GO_MLIB_TEST_FPOS
#undef C2GO_MLIB_TEST_FILE
