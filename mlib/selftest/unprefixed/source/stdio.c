/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/stdio.h>
#include <wchar.h> /* Must keep string-only wide APIs without exposing root FILE I/O. */

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_stdio_test_gc(void);

#define C2GO_MLIB_TEST_FILE FILE
#define C2GO_MLIB_TEST_FPOS fpos_t
#define C2GO_MLIB_TEST_FOPEN fopen
#define C2GO_MLIB_TEST_FDOPEN fdopen
#define C2GO_MLIB_TEST_FMEMOPEN fmemopen
#define C2GO_MLIB_TEST_FCLOSE fclose
#define C2GO_MLIB_TEST_FFLUSH fflush
#define C2GO_MLIB_TEST_FTELL ftell
#define C2GO_MLIB_TEST_FSEEK fseek
#define C2GO_MLIB_TEST_FGETPOS fgetpos
#define C2GO_MLIB_TEST_FSETPOS fsetpos
#define C2GO_MLIB_TEST_FREAD fread
#define C2GO_MLIB_TEST_FPUTS fputs
#define C2GO_MLIB_TEST_FPUTC fputc
#define C2GO_MLIB_TEST_FGETC fgetc
#define C2GO_MLIB_TEST_GETDELIM getdelim
#define C2GO_MLIB_TEST_GETLINE getline
#define C2GO_MLIB_TEST_GETC getc
#define C2GO_MLIB_TEST_UNGETC ungetc
#define C2GO_MLIB_TEST_FEOF feof
#define C2GO_MLIB_TEST_FERROR ferror
#define C2GO_MLIB_TEST_CLEARERR clearerr
#define C2GO_MLIB_TEST_FILENO fileno
#define C2GO_MLIB_TEST_FPRINTF fprintf
#define C2GO_MLIB_TEST_FLOCKFILE flockfile
#define C2GO_MLIB_TEST_FUNLOCKFILE funlockfile
#define C2GO_MLIB_TEST_STDIN stdin
#define C2GO_MLIB_TEST_STDOUT stdout
#define C2GO_MLIB_TEST_PRINTF printf
#define C2GO_MLIB_TEST_PUTCHAR putchar
#define C2GO_MLIB_TEST_PUTS puts
#define C2GO_MLIB_TEST_GETCHAR getchar
#define C2GO_MLIB_TEST_SCANF scanf
#define C2GO_MLIB_TEST_VSCANF vscanf
#define C2GO_MLIB_TEST_FSCANF fscanf
#define C2GO_MLIB_TEST_VFSCANF vfscanf
#define C2GO_MLIB_TEST_SSCANF sscanf
#define C2GO_MLIB_TEST_VSSCANF vsscanf
#define C2GO_MLIB_TEST_EXPORT mlib_stdio_unprefixed_selftest
#define C2GO_MLIB_TEST_STDOUT_EXPORT mlib_stdio_unprefixed_stdout_selftest
#define C2GO_MLIB_TEST_STDIN_EXPORT mlib_stdio_unprefixed_stdin_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_stdio_test_gc()
#include "../../source/stdio_fixture.inc"

#pragma c2go pop
