/* bits/alltypes.h — c2go-libc's single source of truth for ALL fundamental
 * types (modelled on musl's bits/alltypes.h).
 *
 * ON-DEMAND EXPOSURE (musl's __NEED_ mechanism — including this file leaks
 * nothing): a type is materialised in a translation unit ONLY if some header
 * requested it. A public header does, before it uses a type:
 *       #define __NEED_size_t
 *       #define __NEED_...           (exactly the types it must expose)
 *       #include <bits/alltypes.h>
 * The per-type __DEFINED_* guard makes a type that several headers request
 * define exactly once.
 *
 * There is DELIBERATELY no whole-file include guard: a later header in the
 * same TU must be able to re-include this file with a fresh #define __NEED_...
 * to pull in a type an earlier include did not request.
 *
 * WIDTH CLASSES — never conflated (spelled via clang's target-aware builtins,
 * not naked long/long long, so each type lands in the right class on every
 * data model ILP32/LP64/LLP64 and in the platform's native spelling):
 *   POINTER-SIZED   size_t ssize_t ptrdiff_t intptr_t uintptr_t == sizeof(void*)
 *   EXACTLY-64-BIT  int64_t off_t time_t ...                    == 8 everywhere
 *   EXACTLY-N       int8/16/32_t                                == 1/2/4
 * Matching c2go.h's own __SIZE_TYPE__/__INTPTR_TYPE__ spellings means libc
 * types cross the GC / runtime bridge with no width truncation. Types with a
 * native per-OS shape (wchar_t) use the matching builtin so they follow the
 * target (see ../PORTABILITY.md). A struct references member types directly,
 * so a header requesting the struct must also __NEED_ those (e.g. time_t for
 * struct timespec).
 */

/* ── pointer-sized (POINTER-SIZED width class) ─────────────────────── */
#if defined(__NEED_size_t) && !defined(__DEFINED_size_t)
typedef __SIZE_TYPE__    size_t;
#define __DEFINED_size_t
#endif
#if defined(__NEED_ssize_t) && !defined(__DEFINED_ssize_t)
typedef __INTPTR_TYPE__  ssize_t;   /* signed pointer-sized */
#define __DEFINED_ssize_t
#endif
#if defined(__NEED_ptrdiff_t) && !defined(__DEFINED_ptrdiff_t)
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define __DEFINED_ptrdiff_t
#endif
#if defined(__NEED_intptr_t) && !defined(__DEFINED_intptr_t)
typedef __INTPTR_TYPE__  intptr_t;
#define __DEFINED_intptr_t
#endif
#if defined(__NEED_uintptr_t) && !defined(__DEFINED_uintptr_t)
typedef __UINTPTR_TYPE__ uintptr_t;
#define __DEFINED_uintptr_t
#endif

/* ── fixed-width integers (EXACTLY-N / EXACTLY-64) ─────────────────── */
#if defined(__NEED_int8_t) && !defined(__DEFINED_int8_t)
typedef __INT8_TYPE__    int8_t;
#define __DEFINED_int8_t
#endif
#if defined(__NEED_int16_t) && !defined(__DEFINED_int16_t)
typedef __INT16_TYPE__   int16_t;
#define __DEFINED_int16_t
#endif
#if defined(__NEED_int32_t) && !defined(__DEFINED_int32_t)
typedef __INT32_TYPE__   int32_t;
#define __DEFINED_int32_t
#endif
#if defined(__NEED_int64_t) && !defined(__DEFINED_int64_t)
typedef __INT64_TYPE__   int64_t;
#define __DEFINED_int64_t
#endif
#if defined(__NEED_intmax_t) && !defined(__DEFINED_intmax_t)
typedef __INTMAX_TYPE__  intmax_t;
#define __DEFINED_intmax_t
#endif
#if defined(__NEED_uint8_t) && !defined(__DEFINED_uint8_t)
typedef __UINT8_TYPE__   uint8_t;
#define __DEFINED_uint8_t
#endif
#if defined(__NEED_uint16_t) && !defined(__DEFINED_uint16_t)
typedef __UINT16_TYPE__  uint16_t;
#define __DEFINED_uint16_t
#endif
#if defined(__NEED_uint32_t) && !defined(__DEFINED_uint32_t)
typedef __UINT32_TYPE__  uint32_t;
#define __DEFINED_uint32_t
#endif
#if defined(__NEED_uint64_t) && !defined(__DEFINED_uint64_t)
typedef __UINT64_TYPE__  uint64_t;
#define __DEFINED_uint64_t
#endif
#if defined(__NEED_uintmax_t) && !defined(__DEFINED_uintmax_t)
typedef __UINTMAX_TYPE__ uintmax_t;
#define __DEFINED_uintmax_t
#endif

/* ── wchar_t (native per-OS: int on Linux/macOS, unsigned short on Win) ─ */
#if defined(__NEED_wchar_t) && !defined(__DEFINED_wchar_t)
typedef __WCHAR_TYPE__   wchar_t;
#define __DEFINED_wchar_t
#endif

/* ── wint_t / mbstate_t (multibyte). mbstate_t is a 2-word POD — no pointers,
 * so GC-noscan. Every converter keeps its whole protocol in word 0 (the UTF-8
 * DFA accumulator; the uchar mbrtoc16/c16rtomb pair-pending protocols — #690:
 * the wchar_t engines stall rather than park, so no second word is ever
 * needed); word 1 only keeps the C ABI size right, and musl's 1-word mbsinit
 * test stays correct. */
#if defined(__NEED_wint_t) && !defined(__DEFINED_wint_t)
typedef unsigned         wint_t;
#define __DEFINED_wint_t
#endif
#if defined(__NEED_mbstate_t) && !defined(__DEFINED_mbstate_t)
typedef struct __mbstate_t { unsigned __opaque1, __opaque2; } mbstate_t;
#define __DEFINED_mbstate_t
#endif

/* ── time types (EXACTLY-64-BIT). tv_nsec/suseconds_t are a uniform 64-bit
 * time ABI matching the Go bridge's Timespec/Timeval = {int64,int64}; wider
 * than POSIX `long` on ILP32 by design (the bridge converts on 32-bit). */
#if defined(__NEED_time_t) && !defined(__DEFINED_time_t)
typedef __INT64_TYPE__   time_t;
#define __DEFINED_time_t
#endif
#if defined(__NEED_suseconds_t) && !defined(__DEFINED_suseconds_t)
typedef __INT64_TYPE__   suseconds_t;
#define __DEFINED_suseconds_t
#endif
#if defined(__NEED_clock_t) && !defined(__DEFINED_clock_t)
typedef __INT64_TYPE__   clock_t;
#define __DEFINED_clock_t
#endif
#if defined(__NEED_clockid_t) && !defined(__DEFINED_clockid_t)
typedef int              clockid_t;
#define __DEFINED_clockid_t
#endif
#if defined(__NEED_struct_timespec) && !defined(__DEFINED_struct_timespec)
struct timespec { time_t tv_sec; __INT64_TYPE__ tv_nsec; };
#define __DEFINED_struct_timespec
#endif
#if defined(__NEED_struct_timeval) && !defined(__DEFINED_struct_timeval)
struct timeval { time_t tv_sec; suseconds_t tv_usec; };
#define __DEFINED_struct_timeval
#endif
#if defined(__NEED_struct_winsize) && !defined(__DEFINED_struct_winsize)
/* Identical layout on linux (kernel/musl) and darwin (xnu ttycom.h); the
 * pointer passes raw to the TIOCGWINSZ/TIOCSWINSZ ioctl (#675 stage D). */
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#define __DEFINED_struct_winsize
#endif

/* ── filesystem / id types (mode_t/pid_t/uid_t/gid_t are placeholder
 * widths; revisit per-OS when sys/stat.h etc. land — no clang builtin). */
#if defined(__NEED_off_t) && !defined(__DEFINED_off_t)
typedef __INT64_TYPE__   off_t;   /* 64-bit (LFS) on every model */
#define __DEFINED_off_t
#endif
#if defined(__NEED_regoff_t) && !defined(__DEFINED_regoff_t)
typedef __INT64_TYPE__   regoff_t; /* musl _Addr; explicit int64 so the
                                    * windows LLP64 model (long==4) agrees */
#define __DEFINED_regoff_t
#endif
#if defined(__NEED_mode_t) && !defined(__DEFINED_mode_t)
typedef unsigned         mode_t;
#define __DEFINED_mode_t
#endif
#if defined(__NEED_pid_t) && !defined(__DEFINED_pid_t)
typedef int              pid_t;
#define __DEFINED_pid_t
#endif
#if defined(__NEED_uid_t) && !defined(__DEFINED_uid_t)
typedef unsigned         uid_t;
#define __DEFINED_uid_t
#endif
#if defined(__NEED_gid_t) && !defined(__DEFINED_gid_t)
typedef unsigned         gid_t;
#define __DEFINED_gid_t
#endif
/* stat metadata types — fixed 64-bit; c2go's struct stat is uniform and the
 * Go stat bridge fills it (we never copy a host struct stat), so these need
 * no per-OS shape. */
#if defined(__NEED_dev_t) && !defined(__DEFINED_dev_t)
typedef __UINT64_TYPE__  dev_t;
#define __DEFINED_dev_t
#endif
#if defined(__NEED_ino_t) && !defined(__DEFINED_ino_t)
typedef __UINT64_TYPE__  ino_t;
#define __DEFINED_ino_t
#endif
#if defined(__NEED_nlink_t) && !defined(__DEFINED_nlink_t)
typedef __UINT64_TYPE__  nlink_t;
#define __DEFINED_nlink_t
#endif
#if defined(__NEED_blksize_t) && !defined(__DEFINED_blksize_t)
typedef __INT64_TYPE__   blksize_t;
#define __DEFINED_blksize_t
#endif
#if defined(__NEED_blkcnt_t) && !defined(__DEFINED_blkcnt_t)
typedef __INT64_TYPE__   blkcnt_t;
#define __DEFINED_blkcnt_t
#endif

/* ── struct iovec (readv/writev; needs size_t) ─────────────────────── */
#if defined(__NEED_struct_iovec) && !defined(__DEFINED_struct_iovec)
struct iovec { void *iov_base; size_t iov_len; };
#define __DEFINED_struct_iovec
#endif

/* ── FILE — opaque public forward-decl; full layout in bits/stdio_impl.h ─ */
#if defined(__NEED_FILE) && !defined(__DEFINED_FILE)
typedef struct _c2go_FILE FILE;
#define __DEFINED_FILE
#endif

/* ── va_list (clang builtin) ───────────────────────────────────────── */
#if defined(__NEED_va_list) && !defined(__DEFINED_va_list)
typedef __builtin_va_list va_list;
#define __DEFINED_va_list
#endif

/* ── locale_t — opaque; c2go-libc has only the C.UTF-8 locale (the object
 * behind it is a shell, defined in source/locale.c) ─────────────────── */
#if defined(__NEED_locale_t) && !defined(__DEFINED_locale_t)
typedef struct __locale_struct *locale_t;
#define __DEFINED_locale_t
#endif

/* ── max_align_t — greatest fundamental alignment (target-adaptive: the
 * long double member is 16-aligned on x86_64 SysV, 8 on Windows LLP64) ─ */
#if defined(__NEED_max_align_t) && !defined(__DEFINED_max_align_t)
typedef struct { long long __ll; long double __ld; } max_align_t;
#define __DEFINED_max_align_t
#endif
