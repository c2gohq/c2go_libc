// stat_linux.go — translate a linux syscall.Stat_t into the uniform cStat
// (stat.go). One file covers amd64 and arm64: the field names match across both,
// and the width differences (arm64 Nlink uint32 / Blksize int32 vs amd64 uint64 /
// int64) are absorbed by the conversions below.
//
//go:build linux

package libc

import "syscall"

func statFill(dst *cStat, s *syscall.Stat_t) {
	dst.dev = uint64(s.Dev)
	dst.ino = s.Ino
	dst.nlink = uint64(s.Nlink)
	dst.mode = s.Mode
	dst.uid = s.Uid
	dst.gid = s.Gid
	dst.rdev = uint64(s.Rdev)
	dst.size = s.Size
	dst.blksize = int64(s.Blksize)
	dst.blocks = s.Blocks
	dst.atim = cTimespec{int64(s.Atim.Sec), int64(s.Atim.Nsec)}
	dst.mtim = cTimespec{int64(s.Mtim.Sec), int64(s.Mtim.Nsec)}
	dst.ctim = cTimespec{int64(s.Ctim.Sec), int64(s.Ctim.Nsec)}
}
