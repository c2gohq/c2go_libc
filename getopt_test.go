package libc

// Tranche D: getopt / getopt_long (source/getopt.c, ported from musl). Exercises
// the shared optarg/optind/opterr/optopt globals (the c2go-specific risk: the C
// getopt code and the caller must see the SAME Go global) through the bindings.

import (
	"reflect"
	"runtime"
	"testing"
	"unsafe"
)

//go:linkname goOptarg github.com/c2gohq/c2go_libc.optarg
var goOptarg *byte

//go:linkname goOptind github.com/c2gohq/c2go_libc.optind
var goOptind int32

//go:linkname goOpterr github.com/c2gohq/c2go_libc.opterr
var goOpterr int32

// cOption mirrors C `struct option` { const char *name@0; int has_arg@8;
// int *flag@16; int val@24; } (size 32, 8-aligned).
type cOption struct {
	name   *byte
	hasArg int32
	_      int32
	flag   *int32
	val    int32
	_      int32
}

// makeArgv builds a NULL-terminated C argv from args; the returned slices must be
// kept alive for the duration of the getopt scan.
func makeArgv(args ...string) (argv **byte, ptrs []*byte, keep [][]byte) {
	ptrs = make([]*byte, len(args)+1)
	keep = make([][]byte, len(args))
	for i, a := range args {
		b := append([]byte(a), 0)
		keep[i] = b
		ptrs[i] = &b[0]
	}
	return &ptrs[0], ptrs, keep
}

func TestGetopt(t *testing.T) {
	argv, ptrs, keep := makeArgv("prog", "-ab", "-c", "carg", "arg1")
	optstr := csb("abc:")
	goOptind = 1
	goOpterr = 0 // silence the unrecognised-option message

	type step struct {
		ret int32
		arg string
	}
	var steps []step
	for {
		r := Getopt(5, argv, optstr)
		if r == -1 {
			break
		}
		a := ""
		if goOptarg != nil {
			a = cstr(goOptarg)
		}
		steps = append(steps, step{r, a})
	}
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(keep)

	want := []step{{'a', ""}, {'b', ""}, {'c', "carg"}}
	if !reflect.DeepEqual(steps, want) {
		t.Fatalf("getopt sequence = %v, want %v", steps, want)
	}
	if goOptind != 4 {
		t.Errorf("optind = %d, want 4 (points at arg1)", goOptind)
	}
}

func TestGetoptMissingArg(t *testing.T) {
	// "-c" with no following argument: ':' leading optstring -> return ':'.
	argv, ptrs, keep := makeArgv("prog", "-c")
	optstr := csb(":c:")
	goOptind = 1
	goOpterr = 0
	r := Getopt(2, argv, optstr)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(keep)
	if r != ':' {
		t.Fatalf("getopt missing-arg = %q, want ':'", rune(r))
	}
}

func TestGetoptLong(t *testing.T) {
	vb := append([]byte("verbose"), 0)
	fl := append([]byte("file"), 0)
	opts := []cOption{
		{name: &vb[0], hasArg: 0, val: 'v'},
		{name: &fl[0], hasArg: 1, val: 'f'},
		{}, // terminator (name == NULL)
	}
	argv, ptrs, keep := makeArgv("prog", "--verbose", "--file=out", "pos")
	optstr := csb("")
	goOptind = 1
	goOpterr = 0
	var idx int32
	longp := (*option)(unsafe.Pointer(&opts[0]))

	r1 := GetoptLong(4, argv, optstr, longp, &idx)
	r2 := GetoptLong(4, argv, optstr, longp, &idx)
	arg2 := ""
	if goOptarg != nil {
		arg2 = cstr(goOptarg)
	}
	r3 := GetoptLong(4, argv, optstr, longp, &idx)
	runtime.KeepAlive(ptrs)
	runtime.KeepAlive(keep)
	runtime.KeepAlive(opts)
	runtime.KeepAlive(vb)
	runtime.KeepAlive(fl)

	if r1 != 'v' {
		t.Errorf("getopt_long #1 = %q, want 'v'", rune(r1))
	}
	if r2 != 'f' || arg2 != "out" {
		t.Errorf("getopt_long #2 = %q optarg=%q, want 'f' \"out\"", rune(r2), arg2)
	}
	if r3 != -1 {
		t.Errorf("getopt_long #3 = %d, want -1", r3)
	}
}
