// SPDX-License-Identifier: AGPL-3.0-only

//go:build unix

package posixdir

import (
	"os"
	"syscall"
)

func inode(info os.FileInfo) uint64 {
	if stat, ok := info.Sys().(*syscall.Stat_t); ok {
		return stat.Ino
	}
	return 0
}
