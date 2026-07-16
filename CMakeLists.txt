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
    # multibyte musl-VERBATIM group. wcrtomb is musl's full UTF-8 encoder (a wchar_t-width
    # value is always <= WCHAR_MAX, so it needs no 32-bit scalar core -- see csrc/multibyte.c);
    # wcrtomb (encoder) + mbtowc (non-restartable decoder) are musl's full DFA VERBATIM,
    # each self-contained (mbtowc adds one folding `sizeof(wchar_t)<4` guard). The rest are
    # 1-3 line facades over an engine kept in csrc/multibyte.c (mblen->mbtowc [both in fork
    # now], wctomb->wcrtomb [both in fork], mbstowcs->mbsrtowcs, wcstombs->wcsrtombs,
    # mbrlen->mbrtowc; btowc/wctob use the DFA macros via a c2go-adapted src/multibyte/
    # internal.h). The UTF-16-WOVEN restartable engines they call stay in csrc.
    ${MUSL_DIR}/src/multibyte/wcrtomb.c
    ${MUSL_DIR}/src/multibyte/mbtowc.c
    ${MUSL_DIR}/src/multibyte/mblen.c
    ${MUSL_DIR}/src/multibyte/wctomb.c
    ${MUSL_DIR}/src/multibyte/mbstowcs.c
    ${MUSL_DIR}/src/multibyte/wcstombs.c
    ${MUSL_DIR}/src/multibyte/mbrlen.c
    ${MUSL_DIR}/src/multibyte/btowc.c
    ${MUSL_DIR}/src/multibyte/wctob.c
    # mbsinit: musl's 1-word initial-state test verbatim. Correct as-is because the
    # decode layer keeps ALL its state in mbstate_t word 0 -- the wchar_t engines
    # never split a surrogate pair across calls (pair-stall, see csrc/multibyte.c
    # mbsnrtowcs), and the uchar pending protocols below are word-0 (and correctly
    # non-initial) too.
    ${MUSL_DIR}/src/multibyte/mbsinit.c
    # uchar.h (C11 char16_t/char32_t, #673/#691): musl verbatim except the width
    # policy -- the decoders swap mbrtowc for the __mbrtoc32 scalar core (a 16-bit
    # wchar_t can't carry a supplementary scalar, but char16_t/char32_t must);
    # c32rtomb adds one folding windows block (the ONE 32-bit encoder) and c16rtomb
    # funnels its combined pair through it.
    ${MUSL_DIR}/src/multibyte/mbrtoc16.c
    ${MUSL_DIR}/src/multibyte/mbrtoc32.c
    ${MUSL_DIR}/src/multibyte/c16rtomb.c
    ${MUSL_DIR}/src/multibyte/c32rtomb.c
    # the shared UTF-8 DFA transition table (__fsmu8), musl's own src/multibyte/internal.c.
    # Indexed by mbtowc (above) and the restartable engines in csrc/multibyte.c -- one copy
    # for the whole package, so the musl table data lives in the fork, not vendored into csrc.
    ${MUSL_DIR}/src/multibyte/internal.c
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
    # prng: rand/rand_r + the whole rand48 family, musl verbatim (lock-free pure
    # computation). rand48.h carries the __rand48_step cross-TU linkname; the
    # mutable __seed48[7] DATA definition (__seed48.c, PRISTINE 0-diff) is shared
    # across TUs by plain extern — the __fsmu8 precedent extended to writable
    # data. (The BSD random family is csrc/random.c: its lock weave is a rewrite.)
    ${MUSL_DIR}/src/prng/rand.c
    ${MUSL_DIR}/src/prng/rand_r.c
    ${MUSL_DIR}/src/prng/__rand48_step.c
    ${MUSL_DIR}/src/prng/__seed48.c
    ${MUSL_DIR}/src/prng/drand48.c
    ${MUSL_DIR}/src/prng/lrand48.c
    ${MUSL_DIR}/src/prng/mrand48.c
    ${MUSL_DIR}/src/prng/srand48.c
    ${MUSL_DIR}/src/prng/seed48.c
    ${MUSL_DIR}/src/prng/lcong48.c
    # NOTE: wide-char ctype CASE/ALPHA (iswalpha/towlower/towupper + _l) is NOT built
    # verbatim from musl -- its big Unicode tables (alpha.h/casemap.h ~39KB) ARE delegated
    # to Go's unicode package (the agreed design, mirrors iconv), because Go's unicode is
    # the canonical equivalent. That hybrid lives in ${CSRC_DIR}/wctype.c + wctype.go under
    # C2GO_OWN_SOURCES. (wcwidth, which has no Go equivalent, stays byte-exact musl above.)
    # sort: musl's smoothsort, verbatim. qsort.c = the 3-arg __qsort_r kernel
    # (KEEPCASE cross-TU export; weak_alias qsort_r collapsed to a wrapper) + one
    # <c2go.h> include; its `#include "atomic.h"` resolves to the csrc/include shim
    # (pure bit helpers only). qsort_nr.c = the plain qsort facade, reaching the
    # kernel via the <stdlib.h> __qsort_r linkname decl. CONTRACT (#650, documented
    # at the <stdlib.h> decl): elements must not hold managed Go pointers.
    ${MUSL_DIR}/src/stdlib/qsort.c
    ${MUSL_DIR}/src/stdlib/qsort_nr.c
    # search: musl verbatim (unblocked by the malloc glue; CONTRACT at <search.h>:
    # stored pointers must reference stable unmanaged storage). tsearch.h carries
    # the cross-TU __tsearch_balance linkname (tsearch.c defines KEEPCASE,
    # tdelete.c calls); hsearch.c was patched earlier (weak _r aliases collapsed)
    # and is wired here; insque is the long-wired intrusive linked list.
    ${MUSL_DIR}/src/search/insque.c
    ${MUSL_DIR}/src/search/tsearch.c
    ${MUSL_DIR}/src/search/tfind.c
    ${MUSL_DIR}/src/search/tdelete.c
    ${MUSL_DIR}/src/search/twalk.c
    ${MUSL_DIR}/src/search/tdestroy.c
    ${MUSL_DIR}/src/search/lsearch.c
    ${MUSL_DIR}/src/search/hsearch.c
    # path matching: fnmatch (delta: locale_impl.h dropped, MB_CUR_MAX is the
    # <stdlib.h> constant) and glob (deltas: no passwd layer -- ~user/HOME-less ~
    # take musl's own GLOB_NOMATCH failure path; __strchrnul -> public strchrnul).
    # glob's dir walk uses the csrc dirent layer, sort uses qsort.
    ${MUSL_DIR}/src/regex/fnmatch.c
    ${MUSL_DIR}/src/regex/glob.c
    # POSIX regex (TRE): regcomp/regfree/regexec verbatim (b306b16 base carries
    # upstream's backref_ok validation, newer than the OLD port); regerror drops
    # locale_impl via an identity LCTRANS_CUR shim (strerror precedent); tre-mem's
    # three cross-TU entry points export KEEPCASE, declared via linkname in tre.h
    # (the __-names come from tre.h's own namespacing #defines).
    ${MUSL_DIR}/src/regex/regcomp.c
    ${MUSL_DIR}/src/regex/regexec.c
    ${MUSL_DIR}/src/regex/regerror.c
    ${MUSL_DIR}/src/regex/tre-mem.c
    # scandir + comparators: musl verbatim. The sel/cmp arguments are CALLBACKS
    # invoked from C (cmp through qsort) -- C-only surface, exercised by the
    # selftest driver. alphasort needs strcoll (below); versionsort's strverscmp
    # arrived with the misc batch.
    ${MUSL_DIR}/src/dirent/scandir.c
    ${MUSL_DIR}/src/dirent/alphasort.c
    ${MUSL_DIR}/src/dirent/versionsort.c
    # collation/transform: C.UTF-8-only libc collates by code point, so musl's
    # C-locale bodies are verbatim-correct; each __X_l is KEEPCASE + weak_alias
    # collapsed (the strcoll pattern).
    ${MUSL_DIR}/src/locale/strcoll.c
    ${MUSL_DIR}/src/locale/strxfrm.c
    ${MUSL_DIR}/src/locale/wcscoll.c
    ${MUSL_DIR}/src/locale/wcsxfrm.c
    # getopt + GNU getopt_long: verbatim bodies; getopt.c drops locale_impl/
    # stdio_impl behind identity/no-op shims (__lctrans_cur, FLOCK) and exports
    # __getopt_msg KEEPCASE for getopt_long.c (linkname in <getopt.h>, which also
    # #define-aliases BSD optreset onto musl's __optreset storage — c2go doesn't
    # lower weak_alias; __posix_getopt omitted). The state globals optarg/optind/
    # opterr/optopt/__optpos/__optreset are shared DATA (writable cross-TU extern,
    # the __seed48 mechanics).
    ${MUSL_DIR}/src/misc/getopt.c
    ${MUSL_DIR}/src/misc/getopt_long.c
    # nftw/ftw: file tree walk, unix-only (both TUs compile to empty on the
    # windows target; <ftw.h> #errors there). Deltas: the pthread cancel-state
    # bracket and musl's dead local dirfd override are dropped. The fn callback
    # is C-invoked (qsort shape) -- exercised by the nftw selftest driver.
    ${MUSL_DIR}/src/misc/nftw.c
    ${MUSL_DIR}/src/legacy/ftw.c
    # misc long tail, verbatim: libgen basename/dirname (the __xpg_basename
    # weak_alias is omitted), a64l/l64a, getsubopt, reallocarray. err.c keeps its
    # 8 bodies verbatim behind a lazy __progname bridge (#define onto a getter
    # filled from Go's os.Args[0] basename -- musl fills it in __libc_start_main,
    # which c2go's Go-main entry never runs).
    ${MUSL_DIR}/src/misc/basename.c
    ${MUSL_DIR}/src/misc/dirname.c
    ${MUSL_DIR}/src/misc/a64l.c
    ${MUSL_DIR}/src/misc/getsubopt.c
    ${MUSL_DIR}/src/malloc/reallocarray.c
    ${MUSL_DIR}/src/legacy/err.c
    # final odds, verbatim: ffs/ffsl/ffsll (over the atomic.h shim's ctz) and
    # posix_memalign (validate + wrap aligned_alloc; the power-of-two check lives
    # in aligned_alloc itself, musl's division of labor).
    ${MUSL_DIR}/src/misc/ffs.c
    ${MUSL_DIR}/src/misc/ffsl.c
    ${MUSL_DIR}/src/misc/ffsll.c
    ${MUSL_DIR}/src/malloc/posix_memalign.c
    # ── math (dedicated phase; growing in waves) ──────────────────────────────
    # musl src/math verbatim over the csrc/include/libm.h SHIM (distilled from
    # musl's src/internal/libm.h: 64-bit-double world, no ldshape, the __math_*
    # error paths inlined, the @llvm.floor->libcall self-recursion trap
    # documented). The *l surface is <math.h> #define aliases (long double ==
    # double on every c2go target). floor.c is the phase's validation probe.
    ${MUSL_DIR}/src/math/floor.c
    # fenv: c2go's fenv is SOFT (csrc/include/fenv.h — FE_ALL_EXCEPT == 0,
    # FE_TONEAREST == 0; the Go runtime owns the FPU control state). musl's dummy
    # soft-arch src/fenv/fenv.c serves every call verbatim; fesetround.c is the
    # arch-independent argument-check wrapper over __fesetround.
    ${MUSL_DIR}/src/fenv/fenv.c
    ${MUSL_DIR}/src/fenv/fesetround.c
    # math wave 1: round + manipulation families (OLD math_round.c + math_manip.c
    # surface, 57 TUs + floor above = the same 58 symbols). All verbatim +
    # c2go_extern; remainder(f).c drop the drem/dremf weak_alias (BSD-legacy,
    # c2go has no weak_alias — OLD precedent).
    ${MUSL_DIR}/src/math/ceil.c
    ${MUSL_DIR}/src/math/ceilf.c
    ${MUSL_DIR}/src/math/copysign.c
    ${MUSL_DIR}/src/math/copysignf.c
    ${MUSL_DIR}/src/math/fabs.c
    ${MUSL_DIR}/src/math/fabsf.c
    ${MUSL_DIR}/src/math/fdim.c
    ${MUSL_DIR}/src/math/fdimf.c
    ${MUSL_DIR}/src/math/floorf.c
    ${MUSL_DIR}/src/math/fma.c
    ${MUSL_DIR}/src/math/fmaf.c
    ${MUSL_DIR}/src/math/fmax.c
    ${MUSL_DIR}/src/math/fmaxf.c
    ${MUSL_DIR}/src/math/fmin.c
    ${MUSL_DIR}/src/math/fminf.c
    ${MUSL_DIR}/src/math/fmod.c
    ${MUSL_DIR}/src/math/fmodf.c
    ${MUSL_DIR}/src/math/frexp.c
    ${MUSL_DIR}/src/math/frexpf.c
    ${MUSL_DIR}/src/math/ilogb.c
    ${MUSL_DIR}/src/math/ilogbf.c
    ${MUSL_DIR}/src/math/ldexp.c
    ${MUSL_DIR}/src/math/ldexpf.c
    ${MUSL_DIR}/src/math/llrint.c
    ${MUSL_DIR}/src/math/llrintf.c
    ${MUSL_DIR}/src/math/llround.c
    ${MUSL_DIR}/src/math/llroundf.c
    ${MUSL_DIR}/src/math/logb.c
    ${MUSL_DIR}/src/math/logbf.c
    ${MUSL_DIR}/src/math/lrint.c
    ${MUSL_DIR}/src/math/lrintf.c
    ${MUSL_DIR}/src/math/lround.c
    ${MUSL_DIR}/src/math/lroundf.c
    ${MUSL_DIR}/src/math/modf.c
    ${MUSL_DIR}/src/math/modff.c
    ${MUSL_DIR}/src/math/nan.c
    ${MUSL_DIR}/src/math/nanf.c
    ${MUSL_DIR}/src/math/nearbyint.c
    ${MUSL_DIR}/src/math/nearbyintf.c
    ${MUSL_DIR}/src/math/nextafter.c
    ${MUSL_DIR}/src/math/nextafterf.c
    ${MUSL_DIR}/src/math/nexttoward.c
    ${MUSL_DIR}/src/math/nexttowardf.c
    ${MUSL_DIR}/src/math/remainder.c
    ${MUSL_DIR}/src/math/remainderf.c
    ${MUSL_DIR}/src/math/remquo.c
    ${MUSL_DIR}/src/math/remquof.c
    ${MUSL_DIR}/src/math/rint.c
    ${MUSL_DIR}/src/math/rintf.c
    ${MUSL_DIR}/src/math/round.c
    ${MUSL_DIR}/src/math/roundf.c
    ${MUSL_DIR}/src/math/scalbln.c
    ${MUSL_DIR}/src/math/scalblnf.c
    ${MUSL_DIR}/src/math/scalbn.c
    ${MUSL_DIR}/src/math/scalbnf.c
    ${MUSL_DIR}/src/math/trunc.c
    ${MUSL_DIR}/src/math/truncf.c
    # math wave 2: trig (OLD math_trig.c surface, 16 symbols) + sqrt/sqrtf pulled
    # forward from the pow bucket (asin/acos/asinf/acosf call sqrt — link
    # closure). First cross-TU internal surface: __sin/__cos/__tan/__sindf/
    # __cosdf/__tandf/__rem_pio2(f)/__rem_pio2_large are hidden in musl's
    # src/internal/libm.h — here linkname decls in the shim + C2GO_KEEPCASE
    # definitions. __rsqrt_tab is const cross-TU DATA (plain extern, the
    # __fsmu8 mechanism; sqrt_data.h drops <features.h>/hidden).
    ${MUSL_DIR}/src/math/sin.c
    ${MUSL_DIR}/src/math/cos.c
    ${MUSL_DIR}/src/math/tan.c
    ${MUSL_DIR}/src/math/sincos.c
    ${MUSL_DIR}/src/math/sinf.c
    ${MUSL_DIR}/src/math/cosf.c
    ${MUSL_DIR}/src/math/tanf.c
    ${MUSL_DIR}/src/math/sincosf.c
    ${MUSL_DIR}/src/math/asin.c
    ${MUSL_DIR}/src/math/asinf.c
    ${MUSL_DIR}/src/math/acos.c
    ${MUSL_DIR}/src/math/acosf.c
    ${MUSL_DIR}/src/math/atan.c
    ${MUSL_DIR}/src/math/atanf.c
    ${MUSL_DIR}/src/math/atan2.c
    ${MUSL_DIR}/src/math/atan2f.c
    ${MUSL_DIR}/src/math/sqrt.c
    ${MUSL_DIR}/src/math/sqrtf.c
    ${MUSL_DIR}/src/math/sqrt_data.c
    ${MUSL_DIR}/src/math/__sin.c
    ${MUSL_DIR}/src/math/__cos.c
    ${MUSL_DIR}/src/math/__tan.c
    ${MUSL_DIR}/src/math/__sindf.c
    ${MUSL_DIR}/src/math/__cosdf.c
    ${MUSL_DIR}/src/math/__tandf.c
    ${MUSL_DIR}/src/math/__rem_pio2.c
    ${MUSL_DIR}/src/math/__rem_pio2f.c
    ${MUSL_DIR}/src/math/__rem_pio2_large.c
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
    # multibyte ADAPTED core: only the functions whose body WEAVES our windows UTF-16
    # surrogate logic stay here -- the RESTARTABLE engines (the mbrtowc wrapper +
    # mbsrtowcs/wcsrtombs/mbsnrtowcs/wcsnrtombs, each with surrogate branches woven at
    # several points + the __mbrtoc32 scalar DECODE core + __surrogate_to_utf8) and
    # __ctype_get_mb_cur_max (constant 4, not musl's locale read). Everything verbatim-musl
    # (wcrtomb/mbtowc/mbsinit/mblen/wctomb/mbstowcs/wcstombs/mbrlen/btowc/wctob + the uchar
    # four) is built from the musl fork (src/multibyte/*.c) -- they call the engines here
    # across the dir split. State protocol: mbstate_t word 0 ONLY. The wchar_t engines
    # never split a supplementary pair across calls (pair-stall), so musl's 1-word mbsinit
    # is correct and one engine's DFA state resumes in any other; the uchar
    # mbrtoc16/c16rtomb pending protocols (musl's own, C11-mandated -3 / parked-high) also
    # live in word 0 but are private to their function pair, exactly as upstream.
    ${CSRC_DIR}/multibyte.c
    # stat/fsops filesystem layer: thin C wrappers marshalling into Go syscall shims.
    # posix/windows split (mutually exclusive via _WIN32 -> empty TU on the other OS).
    # Symbol sets are disjoint from io_posix.c (bare dup vs dup2/dup3 etc). The Go shims
    # split: added fsops.go/stat.go (unix) + stat_windows.go (windows); the per-GOOS
    # helpers (fsops_darwin/linux.go, stat_darwin/linux.go, stat_types.go) already arrived
    # with the fd/io cluster as dormant exports and are activated here. No x/sys (the
    # x/sys-pulling siblings fallocate/futimens/pipe2/ioctl stay deferred). stat2.c's
    # utimensat/futimens/mkfifo/fallocate shims are undefined until that deferred cluster.
    # BSD random family: REWRITTEN from musl src/prng/random.c — the internal
    # spinlock (lock.h) is replaced by a pthread_mutex woven through all four
    # entry points, so it is csrc (the lock-free rand48 family is fork-verbatim).
    ${CSRC_DIR}/random.c
    # uname: musl's is a raw SYS_uname one-liner; this libc cannot issue raw
    # syscalls, so the same shape runs over the __c2go_syscall_uname Go bridge
    # (uname_unix.go marshals per-OS x/sys field widths into the uniform
    # musl-shaped struct). Empty TU on windows.
    ${CSRC_DIR}/uname.c
    # unix fd/path syscall surface: musl one-liner shapes (call the shim, apply
    # __syscall_ret) over unistd_shim_unix.go / pipe2_{linux,darwin}.go —
    # readv/writev, the *at family, fchdir/f/l/chown, pipe2, sync. Unix-only.
    ${CSRC_DIR}/unistd2.c
    # conf bridges (cross-platform Go primitives, incl. windows):
    # gethostname/getentropy/getpagesize/sysconf over conf_bridge.go.
    ${CSRC_DIR}/conf.c
    # ioctl: the WHITELISTED entry (ioctl is variadic and the c2go vararg pack is
    # TYPED -- each supported command's argument kind is known here; unknown
    # commands are refused ENOTTY before reaching the kernel). Raw pass-through to
    # the SYS_ioctl shim (ioctl_unix.go, x/sys). Unix-only.
    ${CSRC_DIR}/ioctl.c
    # termios: per-OS ADAPTED (linux = musl src/termios shapes; darwin = Apple
    # Libc shapes over the xnu TIOCGETA command set -- no TCGETS there), all over
    # the whitelisted ioctl. tcgetpgrp/tcsetpgrp live here per-OS too (darwin
    # needs a POSIX-conformance gate musl doesn't). Empty TU on windows.
    ${CSRC_DIR}/termios.c
    # process control: the atexit/at_quick_exit registry stays in C (registered
    # c2go fps run natively; only the final stop crosses to Go's os.Exit).
    # __c2go_finalize = the shared LIFO-drain+fflush(NULL) shutdown core, also a
    # Go-callable export for hand-written hosts. abort runs a caught SIGABRT
    # handler synchronously (signal.c's KEEPCASE dispatch) then always dies.
    # _Exit is a direct <stdlib.h> linkname onto __c2go_exit (dodges the
    # c2go-bind clash where _Exit and exit both fold to Go "Exit").
    ${CSRC_DIR}/exit.c
    # signal: the C side owns the sig->handler table and dispatch; Go bridges
    # delivery (os/signal Notify pump -> C2goSignalRun, portable incl. windows
    # console-ctrl). Hardware-fault signals the Go runtime owns are refused
    # SIG_ERR honestly; raise = kernel self-signal on unix, sync C dispatch on
    # windows.
    ${CSRC_DIR}/signal.c
    # locale: the single-C.UTF-8-locale policy layer -- setlocale honestly
    # refuses other names, localeconv serves the static posix lconv, nl_langinfo
    # the C-locale strings, newlocale/uselocale the trivial one-locale model,
    # strfmon the monetary engine, strto*_l the forwarding variants. (The
    # verbatim collation four live in the fork, src/locale/.)
    ${CSRC_DIR}/locale.c
    # intl: the gettext compatibility layer (identity translations -- no
    # catalogs; textdomain/bindtextdomain bookkeeping is honest no-op state).
    ${CSRC_DIR}/intl.c
    # iconv: thin C surface over the Go x/text delegation (iconv.go) -- the
    # agreed big-table direction (same line as the wctype unicode bridge).
    ${CSRC_DIR}/iconv.c
    # resource: getrusage/getrlimit over per-OS x/sys bridges (musl one-liner
    # shapes). Unix-only (empty TU on windows).
    ${CSRC_DIR}/resource.c
    # strsignal: per-OS ADAPTED (musl's compile-time linux-numbering chain is
    # reshaped around our per-OS <signal.h> exports; _NSIG pinned locally;
    # LCTRANS identity). Unix-only. strtoimax/strtoumax: plain forwarders onto
    # strtoll/strtoull (the strtol family lives inside the stdio blob).
    ${CSRC_DIR}/strsignal.c
    ${CSRC_DIR}/inttypes.c
    # dirent: directory streams over the Go handle table (DIR = handle + entry
    # buffer; enumeration is os.File.ReadDir via dirent.go, cross-platform).
    # dirfd/fdopendir are unix-only (windows os.File.Fd() is a HANDLE, not a CRT
    # fd). Consumed by the fork's scandir/glob across the dir split.
    ${CSRC_DIR}/dirent.c
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
