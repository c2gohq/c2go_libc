// dirent_windows.go — #678: windows has no inode surface on DirEntry.Info
// (Win32FileAttributeData carries none); d_ino stays the synthetic sequence.
//go:build windows

package libc

import "os"

func sysInode(os.FileInfo) uint64 { return 0 }
