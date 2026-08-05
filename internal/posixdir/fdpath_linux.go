// SPDX-License-Identifier: AGPL-3.0-only

//go:build linux

package posixdir

import (
	"os"
	"strconv"
)

func fdPath(fd int32) string {
	path, err := os.Readlink("/proc/self/fd/" + strconv.Itoa(int(fd)))
	if err != nil {
		return ""
	}
	return path
}
