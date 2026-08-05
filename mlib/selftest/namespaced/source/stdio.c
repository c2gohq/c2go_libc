/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/stdio.h>
#include <stdio.h>
#include <wchar.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_stdio_test_gc(void);

#define C2GO_MLIB_TEST_FILE mlib_FILE
#define C2GO_MLIB_TEST_FPOS mlib_fpos_t
#define C2GO_MLIB_TEST_FOPEN mlib_fopen
#define C2GO_MLIB_TEST_FDOPEN mlib_fdopen
#define C2GO_MLIB_TEST_FMEMOPEN mlib_fmemopen
#define C2GO_MLIB_TEST_OPEN_MEMSTREAM mlib_open_memstream
#define C2GO_MLIB_TEST_FCLOSE mlib_fclose
#define C2GO_MLIB_TEST_FFLUSH mlib_fflush
#define C2GO_MLIB_TEST_FTELL mlib_ftell
#define C2GO_MLIB_TEST_FSEEK mlib_fseek
#define C2GO_MLIB_TEST_FGETPOS mlib_fgetpos
#define C2GO_MLIB_TEST_FSETPOS mlib_fsetpos
#define C2GO_MLIB_TEST_FREAD mlib_fread
#define C2GO_MLIB_TEST_FPUTS mlib_fputs
#define C2GO_MLIB_TEST_FPUTC mlib_fputc
#define C2GO_MLIB_TEST_FGETC mlib_fgetc
#define C2GO_MLIB_TEST_GETDELIM mlib_getdelim
#define C2GO_MLIB_TEST_GETLINE mlib_getline
#define C2GO_MLIB_TEST_GETC mlib_getc
#define C2GO_MLIB_TEST_UNGETC mlib_ungetc
#define C2GO_MLIB_TEST_FEOF mlib_feof
#define C2GO_MLIB_TEST_FERROR mlib_ferror
#define C2GO_MLIB_TEST_CLEARERR mlib_clearerr
#define C2GO_MLIB_TEST_FILENO mlib_fileno
#define C2GO_MLIB_TEST_FPRINTF mlib_fprintf
#define C2GO_MLIB_TEST_FLOCKFILE mlib_flockfile
#define C2GO_MLIB_TEST_FUNLOCKFILE mlib_funlockfile
#define C2GO_MLIB_TEST_STDIN mlib_stdin
#define C2GO_MLIB_TEST_STDOUT mlib_stdout
#define C2GO_MLIB_TEST_PRINTF mlib_printf
#define C2GO_MLIB_TEST_PUTCHAR mlib_putchar
#define C2GO_MLIB_TEST_PUTS mlib_puts
#define C2GO_MLIB_TEST_GETCHAR mlib_getchar
#define C2GO_MLIB_TEST_SCANF mlib_scanf
#define C2GO_MLIB_TEST_VSCANF mlib_vscanf
#define C2GO_MLIB_TEST_FSCANF mlib_fscanf
#define C2GO_MLIB_TEST_VFSCANF mlib_vfscanf
#define C2GO_MLIB_TEST_SSCANF mlib_sscanf
#define C2GO_MLIB_TEST_VSSCANF mlib_vsscanf
#define C2GO_MLIB_TEST_EXPORT mlib_stdio_prefixed_selftest
#define C2GO_MLIB_TEST_STDOUT_EXPORT mlib_stdio_prefixed_stdout_selftest
#define C2GO_MLIB_TEST_STDIN_EXPORT mlib_stdio_prefixed_stdin_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_stdio_test_gc()
#include "../../source/stdio_fixture.inc"

#pragma c2go pop
