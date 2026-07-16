cmake_minimum_required(VERSION 3.20)
project(c2go_libc2 C)

# c2go-libc2 build manifest.
#
# Layout — the Go module lives at the REPO ROOT so the module path stays
# github.com/c2gohq/c2go_libc (underscore) and linknames keep the form
# "github.com/c2gohq/c2go_libc.Fopen" with NO subdir suffix (see MIGRATION_PLAN D2b):
#
#   <root>/        go.mod + the shipped Go module: hand-written + GENERATED *.go/*.s.
#   musl/          our musl fork. LOCAL SYMLINK (-> ../musl) for now; becomes a git
#                  submodule at push time. c2go changes edited IN PLACE on musl's own
#                  file layout (minimal diff). Excluded from the module zip (submodule
#                  contents never ship).
#   csrc/          our OWN C (no musl basis). Gets its own go.mod (nested module) so it is
#                  excluded from the module zip — the .c are build inputs; the shipped
#                  artifact is the generated *.go/*.s at the root.
#
# This file lists ONLY the TUs we actually build. The c2go toolchain (clang -fc2go ->
# c2go-lto -> c2go-bind) consumes this list and emits Go-linkable *.go/*.s into the root.

set(MUSL_DIR ${CMAKE_SOURCE_DIR}/musl)
set(CSRC_DIR ${CMAKE_SOURCE_DIR}/csrc)
set(GOMOD_ROOT ${CMAKE_SOURCE_DIR})   # generated *.go/*.s land here (= the module root)

# --- musl translation units we build (populated during migration) -------------
set(C2GO_MUSL_SOURCES
    # string (mem/str/wcs). memcpy/memmove/memset/bzero + wmem* ARE built here (musl
    # verbatim): they are the routing targets of the c2go-memcpy-typing pass (@llvm.mem*
    # -> the lowercase C symbol for an unmanaged dst; runtime.typedmemmove for a managed
    # one). musl's bodies are scalar (no aggregate copy / struct init), so they never
    # self-route. Only memmove carries a defensive no_builtin -- it is NOT in LoopIdiom's
    # name-guard list (unlike memset/memcpy/strlen); a no-op in this pipeline.
    ${MUSL_DIR}/src/string/memcpy.c
    ${MUSL_DIR}/src/string/memmove.c
    ${MUSL_DIR}/src/string/memset.c
    ${MUSL_DIR}/src/string/bzero.c
    ${MUSL_DIR}/src/string/wmemcpy.c
    ${MUSL_DIR}/src/string/wmemmove.c
    ${MUSL_DIR}/src/string/wmemset.c
    ${MUSL_DIR}/src/string/strlen.c
    ${MUSL_DIR}/src/string/memchr.c
    ${MUSL_DIR}/src/string/memcmp.c
    ${MUSL_DIR}/src/string/memrchr.c
    ${MUSL_DIR}/src/string/memccpy.c
    ${MUSL_DIR}/src/string/mempcpy.c
    ${MUSL_DIR}/src/string/memmem.c
    ${MUSL_DIR}/src/string/strnlen.c
    ${MUSL_DIR}/src/string/strcmp.c
    ${MUSL_DIR}/src/string/strncmp.c
    ${MUSL_DIR}/src/string/strcpy.c
    ${MUSL_DIR}/src/string/stpcpy.c
    ${MUSL_DIR}/src/string/strncpy.c
    ${MUSL_DIR}/src/string/stpncpy.c
    ${MUSL_DIR}/src/string/strlcpy.c
    ${MUSL_DIR}/src/string/strlcat.c
    ${MUSL_DIR}/src/string/strcat.c
    ${MUSL_DIR}/src/string/strncat.c
    ${MUSL_DIR}/src/string/strchr.c
    ${MUSL_DIR}/src/string/strchrnul.c
    ${MUSL_DIR}/src/string/strrchr.c
    ${MUSL_DIR}/src/string/strspn.c
    ${MUSL_DIR}/src/string/strcspn.c
    ${MUSL_DIR}/src/string/strpbrk.c
    ${MUSL_DIR}/src/string/strstr.c
    ${MUSL_DIR}/src/string/strcasestr.c
    ${MUSL_DIR}/src/string/strdup.c
    ${MUSL_DIR}/src/string/strndup.c
    ${MUSL_DIR}/src/string/strtok.c
    ${MUSL_DIR}/src/string/strtok_r.c
    ${MUSL_DIR}/src/string/strsep.c
    ${MUSL_DIR}/src/string/strcasecmp.c
    ${MUSL_DIR}/src/string/strncasecmp.c
    ${MUSL_DIR}/src/string/strerror_r.c
    # strerror lives in musl src/errno/; it is musl's struct+offsetof message-table
    # lookup VERBATIM (the __strerror_l body), with only the LC_MESSAGES translation
    # layer neutralised (we ship no catalogs) -- a minimal in-place patch, not a csrc
    # rewrite. __strerror.h carries musl's table + per-name #ifdef guards (targets'
    # errno name sets differ). strerror_r.c above stays musl's (it calls strerror).
    ${MUSL_DIR}/src/errno/strerror.c
    ${MUSL_DIR}/src/string/strverscmp.c
    ${MUSL_DIR}/src/string/swab.c
    ${MUSL_DIR}/src/string/wcslen.c
    ${MUSL_DIR}/src/string/wcsnlen.c
    ${MUSL_DIR}/src/string/wcscpy.c
    ${MUSL_DIR}/src/string/wcpcpy.c
    ${MUSL_DIR}/src/string/wcsncpy.c
    ${MUSL_DIR}/src/string/wcpncpy.c
    ${MUSL_DIR}/src/string/wcscat.c
    ${MUSL_DIR}/src/string/wcsncat.c
    ${MUSL_DIR}/src/string/wcscmp.c
    ${MUSL_DIR}/src/string/wcsncmp.c
    ${MUSL_DIR}/src/string/wcschr.c
    ${MUSL_DIR}/src/string/wcsrchr.c
    ${MUSL_DIR}/src/string/wcsstr.c
    ${MUSL_DIR}/src/string/wcswcs.c
    ${MUSL_DIR}/src/string/wcsspn.c
    ${MUSL_DIR}/src/string/wcscspn.c
    ${MUSL_DIR}/src/string/wcspbrk.c
    ${MUSL_DIR}/src/string/wcstok.c
    ${MUSL_DIR}/src/string/wcsdup.c
    ${MUSL_DIR}/src/string/wmemchr.c
    ${MUSL_DIR}/src/string/wmemcmp.c
    ${MUSL_DIR}/src/string/wcscasecmp.c
    ${MUSL_DIR}/src/string/wcsncasecmp.c
    ${MUSL_DIR}/src/string/wcscasecmp_l.c
    ${MUSL_DIR}/src/string/wcsncasecmp_l.c
    # ctype
    ${MUSL_DIR}/src/ctype/isalpha.c
    ${MUSL_DIR}/src/ctype/isdigit.c
    ${MUSL_DIR}/src/ctype/isalnum.c
    ${MUSL_DIR}/src/ctype/isblank.c
    ${MUSL_DIR}/src/ctype/iscntrl.c
    ${MUSL_DIR}/src/ctype/isgraph.c
    ${MUSL_DIR}/src/ctype/islower.c
    ${MUSL_DIR}/src/ctype/isprint.c
    ${MUSL_DIR}/src/ctype/ispunct.c
    ${MUSL_DIR}/src/ctype/isspace.c
    ${MUSL_DIR}/src/ctype/isupper.c
    ${MUSL_DIR}/src/ctype/isxdigit.c
    ${MUSL_DIR}/src/ctype/isascii.c
    ${MUSL_DIR}/src/ctype/tolower.c
    ${MUSL_DIR}/src/ctype/toupper.c
    ${MUSL_DIR}/src/ctype/toascii.c
    # wcwidth/wcswidth: musl's bitmap-table column-width algorithm VERBATIM (nonspacing.h
    # + wide.h are musl's own ~5KB tables). Unlike iswalpha/tow* there is no canonical Go
    # equivalent (stdlib unicode has no East-Asian-Width data), so this is NOT delegated
    # to Go -- it is byte-exact musl, hence a minimal in-place patch, not a csrc vendoring.
    ${MUSL_DIR}/src/ctype/wcwidth.c
    ${MUSL_DIR}/src/ctype/wcswidth.c
    # wide-ctype classifiers: musl's algorithm VERBATIM (arithmetic ranges + the
    # White_Space list + the composites/dispatch). They call the ADAPTED lookups
    # (iswalpha/tow* Go-delegated, iswpunct derived) in csrc/wctype.c across the
    # musl/csrc directory split -- same package, same layer. Only iswalpha/towlower/
    # towupper/iswpunct stay in csrc (their bodies are our adaptation); everything
    # else is byte-exact musl, hence in-place patches here. musl's iswalpha.c/
    # iswpunct.c/towctrans.c (the alpha.h/punct.h/casemap.h table impls) are NOT built.
    ${MUSL_DIR}/src/ctype/iswdigit.c
    ${MUSL_DIR}/src/ctype/iswxdigit.c
    ${MUSL_DIR}/src/ctype/iswcntrl.c
    ${MUSL_DIR}/src/ctype/iswprint.c
    ${MUSL_DIR}/src/ctype/iswspace.c
    ${MUSL_DIR}/src/ctype/iswblank.c
    ${MUSL_DIR}/src/ctype/iswalnum.c
    ${MUSL_DIR}/src/ctype/iswgraph.c
    ${MUSL_DIR}/src/ctype/iswlower.c
    ${MUSL_DIR}/src/ctype/iswupper.c
    ${MUSL_DIR}/src/ctype/iswctype.c
    ${MUSL_DIR}/src/ctype/wctrans.c
    # stdlib (integer arithmetic + ascii-to-number + bsearch)
    ${MUSL_DIR}/src/stdlib/abs.c
    ${MUSL_DIR}/src/stdlib/labs.c
    ${MUSL_DIR}/src/stdlib/llabs.c
    ${MUSL_DIR}/src/stdlib/imaxabs.c
    ${MUSL_DIR}/src/stdlib/div.c
    ${MUSL_DIR}/src/stdlib/ldiv.c
    ${MUSL_DIR}/src/stdlib/lldiv.c
    ${MUSL_DIR}/src/stdlib/imaxdiv.c
    ${MUSL_DIR}/src/stdlib/bsearch.c
    ${MUSL_DIR}/src/stdlib/atoi.c
    ${MUSL_DIR}/src/stdlib/atol.c
    ${MUSL_DIR}/src/stdlib/atoll.c
    # prng
    ${MUSL_DIR}/src/prng/rand.c
    ${MUSL_DIR}/src/prng/rand_r.c
    # NOTE: wide-char ctype CASE/ALPHA (iswalpha/towlower/towupper + _l) is NOT built
    # verbatim from musl -- its big Unicode tables (alpha.h/casemap.h ~39KB) ARE delegated
    # to Go's unicode package (the agreed design, mirrors iconv), because Go's unicode is
    # the canonical equivalent. That hybrid lives in ${CSRC_DIR}/wctype.c + wctype.go under
    # C2GO_OWN_SOURCES. (wcwidth, which has no Go equivalent, stays byte-exact musl above.)
    # search (intrusive linked-list; hsearch/tsearch deferred: need malloc glue)
    ${MUSL_DIR}/src/search/insque.c
    # time: the pure calendar/format code kept in place with a minimal annotation-only
    # diff. The UTC math (__secs_to_tm/__tm_to_secs/__year_to_secs/__month_to_secs) is
    # musl verbatim; gmtime/timegm/difftime/ctime*/asctime*/localtime carry only
    # c2go_extern + the __tm_gmtoff/__tm_zone -> tm_gmtoff/tm_zone rename; the
    # strftime/strptime/wcsftime engines stay verbatim and route langinfo through the
    # C-locale __c2go_langinfo.h shim (locale cluster not yet built). The Go-bridge /
    # zoneinfo-replacing functions (clock family, tzset, localtime_r, mktime) are NOT
    # here -- they are wholesale rewrites and live in ${CSRC_DIR}/time.c.
    ${MUSL_DIR}/src/time/__secs_to_tm.c
    ${MUSL_DIR}/src/time/__year_to_secs.c
    ${MUSL_DIR}/src/time/__month_to_secs.c
    ${MUSL_DIR}/src/time/__tm_to_secs.c
    ${MUSL_DIR}/src/time/difftime.c
    ${MUSL_DIR}/src/time/gmtime_r.c
    ${MUSL_DIR}/src/time/gmtime.c
    ${MUSL_DIR}/src/time/localtime.c
    ${MUSL_DIR}/src/time/timegm.c
    ${MUSL_DIR}/src/time/asctime.c
    ${MUSL_DIR}/src/time/ctime_r.c
    ${MUSL_DIR}/src/time/ctime.c
    ${MUSL_DIR}/src/time/strftime.c
    ${MUSL_DIR}/src/time/wcsftime.c
    ${MUSL_DIR}/src/time/strptime.c
    # sleep/usleep live under src/unistd but are pure nanosleep wrappers (this domain).
    ${MUSL_DIR}/src/unistd/sleep.c
    ${MUSL_DIR}/src/unistd/usleep.c
)

# --- our independent C (no musl basis) ----------------------------------------
set(C2GO_OWN_SOURCES
    # wide-char ctype: C/Go hybrid. ASCII in C; the big Unicode tables (alpha/casemap)
    # are delegated to Go's unicode package via wctype.go (agreed design — a large
    # lookup table is Go's to own, mirrors iconv). NOT verbatim musl: that would vendor
    # musl's ~39KB tables. Vendors only the small ~5KB width tables (nonspacing.h/wide.h,
    # in csrc/) since Go has no East-Asian-Width data. See MIGRATION_PLAN.
    ${CSRC_DIR}/wctype.c
    # pthread C glue: the three fp-call trampolines (__c2go_pthread_run{,_void,_dtor} —
    # a pthread start routine / once-init / TSD destructor is a c2go fp, so it must be
    # CALLED in C, the qsort.c pattern) plus the #664 attr-object lifecycle bodies
    # (mutexattr/condattr/rwlockattr init/destroy/settype, pthread_atfork). The Go side
    # of pthreads (mutex/cond/rwlock/sem/key/thread state) is the glue in *.go at root.
    ${CSRC_DIR}/pthread.c
    # raw fd layer: thin C wrappers (read/write/open/close/lseek/fcntl/dup2/dup3/unlink/
    # rmdir/rename) that marshal into one Go syscall shim each (io.go + io_{darwin,linux,
    # windows}.go). posix and windows split; the windows TU delegates std fds 0/1/2 to
    # the Go std-descriptor layer (stdio_std*.go) and the rest to the msvcrt CRT.
    # Unblocks the stdio (FILE*) layer next.
    ${CSRC_DIR}/io_posix.c
    ${CSRC_DIR}/io_windows.c
    # stdio (FILE*) layer: one adapted blob (musl-derived, c2go-modified — the FILE*
    # buffering + printf/scanf engines vfprintf/vfscanf/__floatscan/__intscan/fmt_fp are
    # self-contained file-local statics). Owns the number-conversion family (strtol/strtod/
    # wcsto*) too, so never also add musl strtol.c/strtod.c (would duplicate). Per-FILE
    # lock + FILE handle bridge to Go is stdio.go (root). bits/stdio_impl.h supplies the
    # FILE struct with the c2go lockid field.
    ${CSRC_DIR}/stdio.c
    # multibyte core (adapted blob): mbrtowc/wcrtomb/mbtowc/wctomb/mbstowcs/wcstombs/
    # mbsrtowcs/wcsrtombs/btowc/wctob/mblen/mbrlen/mbsinit + __ctype_get_mb_cur_max +
    # __surrogate_to_utf8. Pure C (no Go glue). Provides the wide-conversion core stdio.c's
    # wide paths + __ctype_get_mb_cur_max depend on. Standard headers only (no internal.h).
    ${CSRC_DIR}/multibyte.c
    # stat/fsops filesystem layer: thin C wrappers marshalling into Go syscall shims.
    # posix/windows split (mutually exclusive via _WIN32 -> empty TU on the other OS).
    # Symbol sets are disjoint from io_posix.c (bare dup vs dup2/dup3 etc). The Go shims
    # split: added fsops.go/stat.go (unix) + stat_windows.go (windows); the per-GOOS
    # helpers (fsops_darwin/linux.go, stat_darwin/linux.go, stat_types.go) already arrived
    # with the fd/io cluster as dormant exports and are activated here. No x/sys (the
    # x/sys-pulling siblings fallocate/futimens/pipe2/ioctl stay deferred). stat2.c's
    # utimensat/futimens/mkfifo/fallocate shims are undefined until that deferred cluster.
    ${CSRC_DIR}/fsops_posix.c
    ${CSRC_DIR}/fsops_windows.c
    ${CSRC_DIR}/stat_posix.c
    ${CSRC_DIR}/stat_windows.c
    ${CSRC_DIR}/stat2.c
    # time (our-own subset): the parts of <time.h> that replace musl's algorithm
    # wholesale rather than patch it -- the clock/sleep family (Go time bridge, not a
    # syscall), the timezone state (time.Local snapshot, not a zoneinfo-DB parser), and
    # localtime_r/mktime (per-instant offset from the Go bridge, not __secs_to_zone).
    # The pure calendar/format code stays in musl src/time (see C2GO_MUSL_SOURCES).
    ${CSRC_DIR}/time.c
    # env (adapted): the C `char **environ` global + putenv/clearenv/__environ_sync.
    # getenv/setenv/unsetenv are Go bridges (process.go) over the os package; the
    # snapshot bridge rebuilds environ (libc-malloc'd, freed on the next sync) so
    # readers of environ see a current view. Also unblocks time_tz_test's Setenv.
    ${CSRC_DIR}/env.c
)

# Include dirs handed to clang -fc2go for every TU above.
set(C2GO_INCLUDE_DIRS
    ${MUSL_DIR}/include
    # ${MUSL_DIR}/arch/<arch> is added per-target by the driver
    ${CSRC_DIR}
)

# NOTE: the custom command that drives clang -fc2go / c2go-lto / c2go-bind into the
# module root is added once the first TU migrates and the pipeline is wired to CMake.
