// fdpath_linux.go — #675: recover a directory path from an fd (fdopendir's
// rewinddir support) via /proc/self/fd.
//go:build linux

package libc

import (
	"os"
	"strconv"
)

func fdDirPath(fd int32) string {
	p, err := os.Readlink("/proc/self/fd/" + strconv.Itoa(int(fd)))
	if err != nil {
		return ""
	}
	return p
}
