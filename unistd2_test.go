//go:build unix

// unistd2_test.go — the unix fd/path syscall surface + conf bridges (#701;
// OLD batch675_test.go's stage-C parts, verbatim): pipe2/readv/writev, the
// *at family + chown, the conf bridges, kill/sched_yield, and the
// dirfd/fdopendir walk primitives (unlocked by the dirent cluster).
package libc

import (
	"os"
	"runtime"
	"testing"
	"unsafe"
)

func TestKillSchedYield(t *testing.T) {
	if rc := Kill(Getpid(), 0); rc != 0 { // existence probe on self
		t.Fatalf("kill(self, 0) = %d", rc)
	}
	if rc := sched_yield(); rc != 0 {
		t.Fatalf("sched_yield = %d", rc)
	}
}

type cIovec struct {
	base unsafe.Pointer
	len  uint64
}

func TestPipe2ReadvWritev(t *testing.T) {
	var fds [2]int32
	if rc := Pipe2(&fds[0], 0); rc != 0 {
		t.Fatalf("pipe2 = %d", rc)
	}
	defer func() { syscallClose(fds[0]); syscallClose(fds[1]) }()
	a, b := []byte("hello "), []byte("world")
	iov := [2]cIovec{
		{unsafe.Pointer(&a[0]), uint64(len(a))},
		{unsafe.Pointer(&b[0]), uint64(len(b))},
	}
	n := Writev(fds[1], (*iovec)(unsafe.Pointer(&iov[0])), 2)
	if n != int64(len(a)+len(b)) {
		t.Fatalf("writev = %d", n)
	}
	r1, r2 := make([]byte, 6), make([]byte, 5)
	riov := [2]cIovec{
		{unsafe.Pointer(&r1[0]), 6},
		{unsafe.Pointer(&r2[0]), 5},
	}
	n = Readv(fds[0], (*iovec)(unsafe.Pointer(&riov[0])), 2)
	if n != 11 || string(r1) != "hello " || string(r2) != "world" {
		t.Fatalf("readv = %d %q %q", n, r1, r2)
	}
}

func TestAtFamilyChown(t *testing.T) {
	dir := t.TempDir()
	sub := append([]byte("d675"), 0)
	df, err := os.Open(dir) // dirfd via os (open() is variadic — not Go-callable)
	if err != nil {
		t.Fatal(err)
	}
	defer df.Close()
	dfd := int32(df.Fd())
	if rc := Mkdirat(dfd, &sub[0], 0o755); rc != 0 {
		t.Fatalf("mkdirat = %d", rc)
	}
	if _, err := os.Stat(dir + "/d675"); err != nil {
		t.Fatal(err)
	}
	from, to := append([]byte("d675"), 0), append([]byte("d675b"), 0)
	if rc := Renameat(dfd, &from[0], dfd, &to[0]); rc != 0 {
		t.Fatalf("renameat = %d", rc)
	}
	const atRemovedir = 0x200 // linux value; darwin 0x80 — pick per OS below
	rmdirFlag := int32(atRemovedir)
	if runtime.GOOS == "darwin" {
		rmdirFlag = 0x80
	}
	if rc := Unlinkat(dfd, &to[0], rmdirFlag); rc != 0 {
		t.Fatalf("unlinkat(AT_REMOVEDIR) = %d", rc)
	}
	// chown to own uid/gid: a no-op that must succeed.
	f := dir + "/own"
	if err := os.WriteFile(f, nil, 0o644); err != nil {
		t.Fatal(err)
	}
	if rc := Chown(csb(f), Getuid(), Getgid()); rc != 0 {
		t.Fatalf("chown = %d", rc)
	}
}

func TestConfBridges(t *testing.T) {
	if ps := Getpagesize(); ps < 4096 {
		t.Fatalf("getpagesize = %d", ps)
	}
	if v := Sysconf(30); int64(v) != int64(Getpagesize()) { // _SC_PAGESIZE
		t.Fatalf("sysconf(_SC_PAGESIZE) = %d", v)
	}
	if v := Sysconf(84); v < 1 { // _SC_NPROCESSORS_ONLN
		t.Fatalf("sysconf(nproc) = %d", v)
	}
	if v := Sysconf(9999); v != -1 { // whitelist miss -> -1/EINVAL
		t.Fatalf("sysconf(bogus) = %d", v)
	}
	var host [256]byte
	if rc := Gethostname(&host[0], 256); rc != 0 || cstr(&host[0]) == "" {
		t.Fatalf("gethostname rc=%d %q", rc, cstr(&host[0]))
	}
	var ent [16]byte
	if rc := Getentropy(unsafe.Pointer(&ent[0]), 16); rc != 0 {
		t.Fatalf("getentropy = %d", rc)
	}
	zero := true
	for _, b := range ent {
		if b != 0 {
			zero = false
		}
	}
	if zero {
		t.Fatal("getentropy returned all zeros")
	}
}

func syscallClose(fd int32) { __c2go_syscall_close(fd) }

func TestDirfdFdopendir(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(dir+"/f1", nil, 0o644); err != nil {
		t.Fatal(err)
	}
	h := __c2go_opendir(csb(dir))
	if h < 0 {
		t.Fatalf("opendir = %d", h)
	}
	fd := __c2go_dirfd(h)
	if fd < 3 {
		t.Fatalf("dirfd = %d", fd)
	}
	__c2go_closedir(h)

	// fdopendir over a fresh directory fd: stream must read f1 (after ./..).
	df, err := os.Open(dir)
	if err != nil {
		t.Fatal(err)
	}
	h2 := __c2go_fdopendir(int32(df.Fd())) // stream takes fd ownership
	if h2 < 0 {
		df.Close()
		t.Fatalf("fdopendir = %d", h2)
	}
	var de cDirent
	seen := map[string]bool{}
	for {
		r := __c2go_readdir(h2, unsafe.Pointer(&de))
		if r <= 0 {
			break
		}
		seen[cstr(&de.name[0])] = true
	}
	__c2go_closedir(h2)
	if !seen["f1"] || !seen["."] || !seen[".."] {
		t.Fatalf("fdopendir stream entries = %v", seen)
	}
}
