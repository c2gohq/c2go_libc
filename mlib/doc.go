// SPDX-License-Identifier: AGPL-3.0-only

// Package mlib implements managed variants of stateful libc APIs.
//
// Unlike the root libc compatibility package, an mlib carrier stores its
// Go-state pointer directly in GC-visible C memory. It therefore needs no
// process-wide ID-to-pointer handle table. C callers use the headers below
// c2go/mlib; managed C allocation must use gc_malloc with c2go_typeinfo, never
// ordinary malloc. The current surface covers unnamed semaphores; pthread
// mutex, condition-variable, and rwlock synchronization; managed directory
// graphs; and managed explicit/standard FILE streams with formatted and
// allocation-returning line I/O, managed memory/custom streams, and wide
// character, string, and formatted input/output I/O, including managed
// popen/pclose process streams.
package mlib
