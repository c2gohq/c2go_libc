// stat_darwin.go — translate a darwin syscall.Stat_t into the uniform cStat
// (stat.go). Darwin's struct has narrow fields (Dev/Rdev int32, Mode/Nlink
// uint16) and per-member timespecs named *timespec; widen each into the uniform
// 64/32-bit layout. Same file serves darwin/amd64 and darwin/arm64 (identical
// Stat_t).
//
//go:build darwin

package libc

import "syscall"

func statFill(dst *cStat, s *syscall.Stat_t) {
	dst.dev = uint64(uint32(s.Dev)) // int32 device id; reinterpret, don't sign-extend
	dst.ino = s.Ino
	dst.nlink = uint64(s.Nlink)
	dst.mode = uint32(s.Mode)
	dst.uid = s.Uid
	dst.gid = s.Gid
	dst.rdev = uint64(uint32(s.Rdev))
	dst.size = s.Size
	dst.blksize = int64(s.Blksize)
	dst.blocks = s.Blocks
	dst.atim = cTimespec{int64(s.Atimespec.Sec), int64(s.Atimespec.Nsec)}
	dst.mtim = cTimespec{int64(s.Mtimespec.Sec), int64(s.Mtimespec.Nsec)}
	dst.ctim = cTimespec{int64(s.Ctimespec.Sec), int64(s.Ctimespec.Nsec)}
}
