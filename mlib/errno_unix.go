// SPDX-License-Identifier: AGPL-3.0-only

//go:build unix

package mlib

import "syscall"

const (
	errEPERM     = int32(syscall.EPERM)
	errEAGAIN    = int32(syscall.EAGAIN)
	errEBUSY     = int32(syscall.EBUSY)
	errEINVAL    = int32(syscall.EINVAL)
	errETIMEDOUT = int32(syscall.ETIMEDOUT)
	errEDEADLK   = int32(syscall.EDEADLK)
	errEOVERFLOW = int32(syscall.EOVERFLOW)
)
