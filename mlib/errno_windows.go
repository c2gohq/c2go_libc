// SPDX-License-Identifier: AGPL-3.0-only

//go:build windows

package mlib

// MinGW-w64 errno values, matching c2go-libc's Windows headers.
const (
	errEPERM     int32 = 1
	errEAGAIN    int32 = 11
	errEBUSY     int32 = 16
	errEINVAL    int32 = 22
	errETIMEDOUT int32 = 138
	errEDEADLK   int32 = 36
	errEOVERFLOW int32 = 132
	errEBADF     int32 = 9
)
