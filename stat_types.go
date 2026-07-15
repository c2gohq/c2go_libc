// stat_types.go — the Go mirror of <sys/stat.h>'s UNIFORM struct stat
// (portable: the layout is identical on every target by design; the per-OS
// work is only the FILL — statFill on unix from syscall.Stat_t, winStatFill
// on Windows from os.FileInfo). Split out of the unix-only stat.go so the
// Windows stat bridges (stat_windows.go) share the same mirror.

package libc

// cTimespec / cStat mirror <sys/stat.h>'s struct timespec / struct stat exactly.
type cTimespec struct {
	sec  int64
	nsec int64
}

type cStat struct {
	dev     uint64    // st_dev    @0
	ino     uint64    // st_ino    @8
	nlink   uint64    // st_nlink  @16
	mode    uint32    // st_mode   @24
	uid     uint32    // st_uid    @28
	gid     uint32    // st_gid    @32
	_pad0   uint32    // hole before the 8-aligned st_rdev @36
	rdev    uint64    // st_rdev   @40
	size    int64     // st_size   @48
	blksize int64     // st_blksize @56
	blocks  int64     // st_blocks @64
	atim    cTimespec // st_atim @72
	mtim    cTimespec // st_mtim @88
	ctim    cTimespec // st_ctim @104
} // 120 bytes
