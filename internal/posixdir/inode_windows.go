// SPDX-License-Identifier: AGPL-3.0-only

//go:build windows

package posixdir

import "os"

func inode(os.FileInfo) uint64 { return 0 }
