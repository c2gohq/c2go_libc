/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_FILE
#error "C2GO_MLIB_TEST_FILE must name the managed FILE type"
#endif
#ifndef C2GO_MLIB_TEST_FMEMOPEN
#error "C2GO_MLIB_TEST_FMEMOPEN must name the managed fmemopen"
#endif
#ifndef C2GO_MLIB_TEST_OPEN_MEMSTREAM
#error "C2GO_MLIB_TEST_OPEN_MEMSTREAM must name the managed open_memstream"
#endif
#ifndef C2GO_MLIB_TEST_COOKIE_IO
#error "C2GO_MLIB_TEST_COOKIE_IO must name the managed cookie callback table"
#endif
#ifndef C2GO_MLIB_TEST_FOPENCOOKIE
#error "C2GO_MLIB_TEST_FOPENCOOKIE must name the managed fopencookie"
#endif
#ifndef C2GO_MLIB_TEST_FWIDE
#error "C2GO_MLIB_TEST_FWIDE must name the managed fwide"
#endif
#ifndef C2GO_MLIB_TEST_FPUTWC
#error "C2GO_MLIB_TEST_FPUTWC must name the managed fputwc"
#endif
#ifndef C2GO_MLIB_TEST_FGETWC
#error "C2GO_MLIB_TEST_FGETWC must name the managed fgetwc"
#endif
#ifndef C2GO_MLIB_TEST_FPUTWS
#error "C2GO_MLIB_TEST_FPUTWS must name the managed fputws"
#endif
#ifndef C2GO_MLIB_TEST_FGETWS
#error "C2GO_MLIB_TEST_FGETWS must name the managed fgetws"
#endif
#ifndef C2GO_MLIB_TEST_UNGETWC
#error "C2GO_MLIB_TEST_UNGETWC must name the managed ungetwc"
#endif
#ifndef C2GO_MLIB_TEST_VFWPRINTF
#error "C2GO_MLIB_TEST_VFWPRINTF must name the managed vfwprintf"
#endif
#ifndef C2GO_MLIB_TEST_FWPRINTF
#error "C2GO_MLIB_TEST_FWPRINTF must name the managed fwprintf"
#endif
#ifndef C2GO_MLIB_TEST_VFWSCANF
#error "C2GO_MLIB_TEST_VFWSCANF must name the managed vfwscanf"
#endif
#ifndef C2GO_MLIB_TEST_FWSCANF
#error "C2GO_MLIB_TEST_FWSCANF must name the managed fwscanf"
#endif
#ifndef C2GO_MLIB_TEST_VSWSCANF
#error "C2GO_MLIB_TEST_VSWSCANF must name the managed vswscanf"
#endif
#ifndef C2GO_MLIB_TEST_SWSCANF
#error "C2GO_MLIB_TEST_SWSCANF must name the managed swscanf"
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
#ifndef C2GO_MLIB_TEST_STDIN
#error "C2GO_MLIB_TEST_STDIN must name the managed standard input stream"
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
#ifndef C2GO_MLIB_TEST_SCANF
#error "C2GO_MLIB_TEST_SCANF must name the managed scanf"
#endif
#ifndef C2GO_MLIB_TEST_VSCANF
#error "C2GO_MLIB_TEST_VSCANF must name the managed vscanf"
#endif
#ifndef C2GO_MLIB_TEST_FSCANF
#error "C2GO_MLIB_TEST_FSCANF must name the managed fscanf"
#endif
#ifndef C2GO_MLIB_TEST_VFSCANF
#error "C2GO_MLIB_TEST_VFSCANF must name the managed vfscanf"
#endif
#ifndef C2GO_MLIB_TEST_SSCANF
#error "C2GO_MLIB_TEST_SSCANF must name the managed sscanf"
#endif
#ifndef C2GO_MLIB_TEST_VSSCANF
#error "C2GO_MLIB_TEST_VSSCANF must name the managed vsscanf"
#endif
#ifndef C2GO_MLIB_TEST_GETDELIM
#error "C2GO_MLIB_TEST_GETDELIM must name the managed getdelim"
#endif
#ifndef C2GO_MLIB_TEST_GETLINE
#error "C2GO_MLIB_TEST_GETLINE must name the managed getline"
#endif
#ifndef C2GO_MLIB_TEST_STDOUT_EXPORT
#error "C2GO_MLIB_TEST_STDOUT_EXPORT must name the standard-output selftest"
#endif
#ifndef C2GO_MLIB_TEST_STDIN_EXPORT
#error "C2GO_MLIB_TEST_STDIN_EXPORT must name the standard-input selftest"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

struct c2go_mlib_scan_result {
    char *file_text;
    char *text;
    wchar_t *wide;
    void *pointer;
    char *line;
    size_t line_capacity;
    char *output;
    size_t output_size;
};

struct c2go_mlib_cookie_state {
    char bytes[128];
    char *managed snapshot;
    size_t position;
    size_t length;
};
typedef struct c2go_mlib_cookie_state *managed
    c2go_mlib_cookie_state_pointer;

static int c2go_mlib_cookie_close_count;

static ssize_t c2go_mlib_cookie_read(void *managed opaque, char *buffer,
                                     size_t length)
{
    c2go_mlib_cookie_state_pointer state = opaque;
    size_t remaining;
    if (state->position >= state->length) return 0;
    remaining = state->length - state->position;
    if (length > remaining) length = remaining;
    if (!state->snapshot) {
        errno = EIO;
        return -1;
    }
    memcpy(buffer, (const void *)(state->snapshot + state->position), length);
    state->position += length;
    return length;
}

static ssize_t c2go_mlib_cookie_write(void *managed opaque,
                                      const char *buffer, size_t length)
{
    c2go_mlib_cookie_state_pointer state = opaque;
    char *managed snapshot;
    if (length > sizeof(state->bytes) - state->position) {
        errno = ENOSPC;
        return -1;
    }
    memcpy(state->bytes + state->position, buffer, length);
    state->position += length;
    if (state->position > state->length) state->length = state->position;
    snapshot = (char *managed)gc_malloc((void *)0, state->length + 1);
    if (!snapshot) {
        errno = ENOMEM;
        return -1;
    }
    memcpy((void *)snapshot, state->bytes, state->length);
    state->snapshot = snapshot;
    return length;
}

static int c2go_mlib_cookie_seek(void *managed opaque, off_t *offset,
                                 int whence)
{
    c2go_mlib_cookie_state_pointer state = opaque;
    off_t base;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = state->position; break;
    case SEEK_END: base = state->length; break;
    default: errno = EINVAL; return -1;
    }
    if (*offset < -base || *offset > (off_t)sizeof(state->bytes) - base) {
        errno = EINVAL;
        return -1;
    }
    state->position = base + *offset;
    *offset = state->position;
    return 0;
}

static int c2go_mlib_cookie_close(void *managed opaque)
{
    (void)opaque;
    ++c2go_mlib_cookie_close_count;
    return 0;
}

static int c2go_mlib_test_vscanf(const char *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VSCANF(format, arguments);
    va_end(arguments);
    return result;
}

static int c2go_mlib_test_vfscanf(C2GO_MLIB_TEST_FILE *stream,
                                  const char *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VFSCANF(stream, format, arguments);
    va_end(arguments);
    return result;
}

static int c2go_mlib_test_vfwprintf(C2GO_MLIB_TEST_FILE *stream,
                                    const wchar_t *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VFWPRINTF(stream, format, arguments);
    va_end(arguments);
    return result;
}

static int c2go_mlib_test_vfwscanf(C2GO_MLIB_TEST_FILE *stream,
                                   const wchar_t *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VFWSCANF(stream, format, arguments);
    va_end(arguments);
    return result;
}

static int c2go_mlib_test_vswscanf(const wchar_t *input,
                                   const wchar_t *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VSWSCANF(input, format, arguments);
    va_end(arguments);
    return result;
}

static int c2go_mlib_test_vsscanf(const char *input,
                                  const char *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VSSCANF(input, format, arguments);
    va_end(arguments);
    return result;
}

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
    const char managed_text[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-long";
    const char memory_text[] = "managed-memory";
    const char cookie_text[] = "managed-cookie";
    const wchar_t wide_text[] = L"wide-stream:42!";
    const wchar_t managed_wide_text[] =
        L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-long";
    char root[512], path[600], input[32] = {0}, formatted[16];
    char word[8] = {0}, set[4] = {0}, mark = 0;
    char pointer_text[32], *target, *line_before, *memory, *output_initial;
    wchar_t wide_input[32] = {0};
    struct c2go_mlib_scan_result *scan;
    c2go_mlib_cookie_state_pointer cookie_state;
    C2GO_MLIB_TEST_COOKIE_IO cookie_functions = {
        c2go_mlib_cookie_read,
        c2go_mlib_cookie_write,
        c2go_mlib_cookie_seek,
        c2go_mlib_cookie_close,
    };
    C2GO_MLIB_TEST_FILE *stream;
    C2GO_MLIB_TEST_FPOS position;
    double fraction = 0;
    int fd, number = 0;
    size_t index;

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
    scan = gc_malloc(c2go_typeinfo(struct c2go_mlib_scan_result),
                     sizeof(*scan));
    if (!scan) return 33;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FSCANF(stream, "%m[a-z]", &scan->file_text) != 1 ||
        strcmp(scan->file_text, "value")) return 21;
    if (C2GO_MLIB_TEST_GETC(stream) != '=') return 22;
    if (C2GO_MLIB_TEST_UNGETC('V', stream) != 'V') return 23;
    if (C2GO_MLIB_TEST_GETC(stream) != 'V') return 24;
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 25;
    if (c2go_mlib_test_vfscanf(stream, "value=%d%c", &number, &mark) != 2 ||
        number != 42 || mark != '!') return 27;

    number = 0;
    if (C2GO_MLIB_TEST_SSCANF("17 2.5 sample", "%d %lf %7s",
                              &number, &fraction, word) != 3 ||
        number != 17 || fraction != 2.5 || strcmp(word, "sample")) return 28;
    if (c2go_mlib_test_vsscanf("%m mp", "%%m %2[mp]", set) != 1 ||
        strcmp(set, "mp")) return 29;

    target = gc_malloc((void *)0, 8);
    if (!target) return 33;
    memcpy(target, "alive", 6);

    /* A long %ms forces multiple managed growth allocations. Both the result
     * object and each pointer field are GC-scanned, so publication must use a
     * write barrier rather than root scanf's ordinary pointer store. */
    if (C2GO_MLIB_TEST_SSCANF(managed_text, "%ms", &scan->text) != 1 ||
        strcmp(scan->text, managed_text)) return 30;
    if (c2go_mlib_test_vsscanf("wide", "%mls", &scan->wide) != 1 ||
        scan->wide[0] != 'w' || scan->wide[4] != 0) return 31;
    if (snprintf(pointer_text, sizeof(pointer_text), "%p", (void *)target) < 1 ||
        c2go_mlib_test_vsscanf(pointer_text, "%p", &scan->pointer) != 1 ||
        scan->pointer != target) return 32;
    target = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (strcmp(scan->text, managed_text) ||
        strcmp((const char *)scan->pointer, "alive")) return 34;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 35;

    /* Force getdelim through more than one managed growth. The returned slot
     * and its capacity live inside a typed GC object; getline then reuses the
     * same allocation for the short tail. */
    stream = C2GO_MLIB_TEST_FOPEN(path, "w+");
    if (!stream) return 36;
    for (index = 0; index < 3072; ++index) {
        if (C2GO_MLIB_TEST_FPUTC('a' + index % 26, stream) == EOF) return 37;
    }
    if (C2GO_MLIB_TEST_FPUTS("|tail", stream) < 0 ||
        C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 38;
    if (C2GO_MLIB_TEST_GETDELIM(&scan->line, &scan->line_capacity, '|', stream)
            != 3073 || scan->line_capacity <= 3073 || scan->line[3072] != '|' ||
        scan->line[3073] != 0) return 39;
    for (index = 0; index < 3072; ++index) {
        if (scan->line[index] != 'a' + index % 26) return 40;
    }
    line_before = scan->line;
    C2GO_MLIB_TEST_FORCE_GC();
    if (scan->line != line_before || scan->line[2048] != 'u') return 41;
    if (C2GO_MLIB_TEST_GETLINE(&scan->line, &scan->line_capacity, stream) != 4 ||
        strcmp(scan->line, "tail") || scan->line != line_before) return 42;
    line_before = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (strcmp(scan->line, "tail")) return 43;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 44;

    /* fmemopen retains a caller-provided managed buffer for the FILE lifetime,
     * even after the caller drops its direct local root. */
    memory = gc_malloc((void *)0, 4096);
    if (!memory) return 45;
    stream = C2GO_MLIB_TEST_FMEMOPEN(memory, 4096, "w+");
    if (!stream) return 46;
    memory = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FPUTS(memory_text, stream) < 0 ||
        C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 47;
    memset(input, 0, sizeof(input));
    if (C2GO_MLIB_TEST_FREAD(input, 1, sizeof(memory_text) - 1, stream)
            != sizeof(memory_text) - 1 || strcmp(input, memory_text)) return 48;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 49;

    /* NULL buffer selects a second GC allocation owned only by the carrier. */
    stream = C2GO_MLIB_TEST_FMEMOPEN((void *)0, 64, "w+");
    if (!stream || C2GO_MLIB_TEST_FPUTS("owned", stream) < 0 ||
        C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 50;
    memset(input, 0, sizeof(input));
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FREAD(input, 1, 5, stream) != 5 ||
        strcmp(input, "owned")) return 51;
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 52;

    /* open_memstream retains both result slots, grows only with GC storage,
     * and republishes the replacement pointer through a write barrier. The
     * slots therefore live in this typed result object rather than on the C
     * stack. */
    stream = C2GO_MLIB_TEST_OPEN_MEMSTREAM(&scan->output,
                                           &scan->output_size);
    if (!stream || !scan->output || scan->output_size != 0 ||
        scan->output[0] != 0) return 53;
    output_initial = scan->output;
    C2GO_MLIB_TEST_FORCE_GC();
    for (index = 0; index < 3072; ++index) {
        if (C2GO_MLIB_TEST_FPUTC('a' + index % 26, stream) == EOF) return 54;
    }
    if (C2GO_MLIB_TEST_FFLUSH(stream) != 0 ||
        scan->output_size != 3072 || scan->output == output_initial ||
        scan->output[3072] != 0) return 55;
    output_initial = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    for (index = 0; index < 3072; ++index) {
        if (scan->output[index] != 'a' + index % 26) return 56;
    }
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_END) != 0 ||
        C2GO_MLIB_TEST_FPUTS("|tail", stream) < 0 ||
        C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 57;
    C2GO_MLIB_TEST_FORCE_GC();
    if (scan->output_size != 3077 || scan->output[3077] != 0 ||
        strcmp(scan->output + 3072, "|tail")) return 58;

    /* The FILE carrier is the only remaining root for the typed cookie while
     * callbacks run. Callback buffer/offset arguments remain borrowed and
     * synchronous. */
    cookie_state = (c2go_mlib_cookie_state_pointer)gc_malloc(
        c2go_typeinfo(struct c2go_mlib_cookie_state), sizeof(*cookie_state));
    if (!cookie_state) return 59;
    c2go_mlib_cookie_close_count = 0;
    stream = C2GO_MLIB_TEST_FOPENCOOKIE(cookie_state, "w+",
                                        cookie_functions);
    if (!stream) return 60;
    cookie_state = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FPUTS(cookie_text, stream) < 0 ||
        C2GO_MLIB_TEST_FFLUSH(stream) != 0 ||
        C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0) return 61;
    memset(input, 0, sizeof(input));
    if (C2GO_MLIB_TEST_FREAD(input, 1, sizeof(cookie_text) - 1, stream)
            != sizeof(cookie_text) - 1 || strcmp(input, cookie_text)) return 62;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0 ||
        c2go_mlib_cookie_close_count != 1) return 63;

    /* Wide FILE operations share root libc's lock-free UTF engine while the
     * mlib wrapper owns the managed carrier lock and buffer root. */
    memory = gc_malloc((void *)0, 256);
    if (!memory) return 64;
    stream = C2GO_MLIB_TEST_FMEMOPEN(memory, 256, "w+");
    if (!stream) return 65;
    memory = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FWIDE(stream, 0) != 0 ||
        C2GO_MLIB_TEST_FWIDE(stream, 1) <= 0) return 66;
    if (C2GO_MLIB_TEST_FPUTWS(L"wide", stream) < 0 ||
        C2GO_MLIB_TEST_FPUTWC(L'-', stream) == WEOF ||
        C2GO_MLIB_TEST_FPUTWS(L"stream", stream) < 0 ||
        C2GO_MLIB_TEST_FWPRINTF(stream, L":%d", 42) != 3 ||
        c2go_mlib_test_vfwprintf(stream, L"%lc", L'!') != 1 ||
        C2GO_MLIB_TEST_FFLUSH(stream) != 0) return 67;
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0 ||
        C2GO_MLIB_TEST_FGETWC(stream) != L'w' ||
        C2GO_MLIB_TEST_UNGETWC(L'w', stream) != L'w' ||
        C2GO_MLIB_TEST_FGETWC(stream) != L'w') return 68;
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0 ||
        C2GO_MLIB_TEST_FGETWS(wide_input, 32, stream) != wide_input ||
        wcscmp(wide_input, wide_text)) return 69;
    number = 0;
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0 ||
        C2GO_MLIB_TEST_FWSCANF(stream, L"wide-stream:%d", &number) != 1 ||
        number != 42 || C2GO_MLIB_TEST_FGETWC(stream) != L'!') return 72;
    memset(wide_input, 0, sizeof(wide_input));
    if (C2GO_MLIB_TEST_FSEEK(stream, 0, SEEK_SET) != 0 ||
        c2go_mlib_test_vfwscanf(stream, L"%15ls", wide_input) != 1 ||
        wcscmp(wide_input, wide_text)) return 73;
    C2GO_MLIB_TEST_FORCE_GC();
    if (C2GO_MLIB_TEST_FCLOSE(stream) != 0) return 70;

    /* Wide string scanning uses the same managed `%m` allocation and pointer
     * publication policy as the stream parser. The long inputs force both the
     * wchar_t and UTF-8 result buffers through managed growth. */
    scan->wide = (void *)0;
    scan->text = (void *)0;
    if (C2GO_MLIB_TEST_SWSCANF(managed_wide_text, L"%mls", &scan->wide) != 1 ||
        wcscmp(scan->wide, managed_wide_text)) return 74;
    if (c2go_mlib_test_vswscanf(managed_wide_text, L"%ms", &scan->text) != 1 ||
        strcmp(scan->text, managed_text)) return 75;
    target = gc_malloc((void *)0, 10);
    if (!target) return 76;
    memcpy(target, "widealive", 10);
    number = snprintf(pointer_text, sizeof(pointer_text), "%p", (void *)target);
    if (number < 1 || number >= 32 ||
        mbstowcs(wide_input, pointer_text, 32) != (size_t)number) return 77;
    if (C2GO_MLIB_TEST_SWSCANF(wide_input, L"%p", &scan->pointer) != 1)
        return 79;
    if (scan->pointer != target) return 80;
    target = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (wcscmp(scan->wide, managed_wide_text) ||
        strcmp(scan->text, managed_text) ||
        strcmp((const char *)scan->pointer, "widealive")) return 78;

    if (unlink(path) || rmdir(root)) return 71;
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
    int first = 0, second = 0;
    /* The Go test deliberately swaps os.Stdin between repeated invocations;
     * clear the prior C stream's sticky EOF status before reading the new
     * live endpoint. Ordinary C code needs the same clearerr after EOF. */
    C2GO_MLIB_TEST_CLEARERR(C2GO_MLIB_TEST_STDIN);
    if (C2GO_MLIB_TEST_GETCHAR() != 'Q') return -1;
    if (c2go_mlib_test_vscanf(" %d", &first) != 1) return -2;
    if (C2GO_MLIB_TEST_SCANF(" %d", &second) != 1) return -3;
    return first + second;
}

#undef C2GO_MLIB_TEST_STDIN_EXPORT
#undef C2GO_MLIB_TEST_STDOUT_EXPORT
#undef C2GO_MLIB_TEST_VSSCANF
#undef C2GO_MLIB_TEST_SSCANF
#undef C2GO_MLIB_TEST_VFSCANF
#undef C2GO_MLIB_TEST_FSCANF
#undef C2GO_MLIB_TEST_VSCANF
#undef C2GO_MLIB_TEST_SCANF
#undef C2GO_MLIB_TEST_GETLINE
#undef C2GO_MLIB_TEST_GETDELIM
#undef C2GO_MLIB_TEST_SWSCANF
#undef C2GO_MLIB_TEST_VSWSCANF
#undef C2GO_MLIB_TEST_FWSCANF
#undef C2GO_MLIB_TEST_VFWSCANF
#undef C2GO_MLIB_TEST_FWPRINTF
#undef C2GO_MLIB_TEST_VFWPRINTF
#undef C2GO_MLIB_TEST_UNGETWC
#undef C2GO_MLIB_TEST_FGETWS
#undef C2GO_MLIB_TEST_FPUTWS
#undef C2GO_MLIB_TEST_FGETWC
#undef C2GO_MLIB_TEST_FPUTWC
#undef C2GO_MLIB_TEST_FWIDE
#undef C2GO_MLIB_TEST_FOPENCOOKIE
#undef C2GO_MLIB_TEST_COOKIE_IO
#undef C2GO_MLIB_TEST_OPEN_MEMSTREAM
#undef C2GO_MLIB_TEST_FMEMOPEN
#undef C2GO_MLIB_TEST_GETCHAR
#undef C2GO_MLIB_TEST_STDIN
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
