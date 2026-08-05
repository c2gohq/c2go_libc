/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_H
#define C2GO_MLIB_H

/* Umbrella include for the managed libc surface implemented so far. */
#include <c2go/mlib/dirent.h>
#include <c2go/mlib/glob.h>
#include <c2go/mlib/pthread.h>
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/stdio.h>
#if !defined(_WIN32)
#include <c2go/mlib/ftw.h>
#endif

#endif /* C2GO_MLIB_H */
