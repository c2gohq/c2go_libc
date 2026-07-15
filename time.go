// time.go — the Go clock bridge behind <time.h>/<sys/time.h> (cross-platform).
//
// c2go-compiled C cannot read a clock itself, so the "now" primitive comes from
// Go's time package. The C wrappers (source/time.c) shape it into time_t /
// struct timespec / struct timeval and own the calendar math. The clock ids are
// c2go-uniform (see <time.h>), interpreted here, NOT host clock_gettime ids.
package libc

import (
	"os"
	"strings"
	"sync"
	"time"
	"unsafe"
)

// monoBase anchors the monotonic clock at package init; time.Since reads the
// process-relative monotonic duration off it.
var monoBase = time.Now()

// ── timezone bridge (behind localtime/mktime/tzset in source/time.c) ──
//
// c2go-compiled C has no timezone database of its own; the local zone offset,
// DST flag, and zone abbreviation all come from Go's time package (time.Local,
// which honours /etc/localtime or $TZ on unix and the OS zone on Windows). The C
// side keeps the musl calendar math and only asks Go "what is the offset here".

var (
	tzZoneMu sync.Mutex
	tzZones  = map[string]*byte{}
	// tzMu guards reassignment of time.Local in tzset(). Time-formatting readers
	// of time.Local are lock-free (a word-sized pointer), as elsewhere in Go.
	tzMu sync.Mutex
)

// internZone returns a stable, C-visible, NUL-terminated copy of a timezone
// abbreviation. The set of distinct abbreviations a process ever sees is tiny
// and bounded, so these malloc'd buffers are interned once and never freed
// (mirrors musl's permanent __tzname storage). The Go heap is non-moving, so the
// pointer stays valid for C to read (via tm_zone / tzname) for the process life.
func internZone(name string) *byte {
	tzZoneMu.Lock()
	defer tzZoneMu.Unlock()
	if p, ok := tzZones[name]; ok {
		return p
	}
	p := (*byte)(Malloc(uint64(len(name) + 1)))
	buf := unsafe.Slice(p, len(name)+1)
	copy(buf, name)
	buf[len(name)] = 0
	tzZones[name] = p
	return p
}

// __c2go_local_offset yields, for an absolute instant `sec` (Unix seconds), the
// local zone's offset east of UTC, its DST flag, and its interned abbreviation.
// localtime[_r] adds `off` to the instant and runs the UTC calendar math on it.
//
//go:linkname __c2go_local_offset
func __c2go_local_offset(sec int64, off *int64, isdst *int32, zone **byte) {
	t := time.Unix(sec, 0).Local()
	name, offset := t.Zone()
	*off = int64(offset)
	if t.IsDST() {
		*isdst = 1
	} else {
		*isdst = 0
	}
	*zone = internZone(name)
}

// __c2go_mktime converts a LOCAL broken-down time to an absolute Unix instant,
// letting time.Date resolve DST and normalise out-of-range fields (exactly what
// POSIX mktime requires). It also reports the resolved offset/DST/zone so the C
// side can refill tm_gmtoff/tm_isdst/tm_zone. year is the full Gregorian year
// and mon is 1..12 (the C caller adds 1900 / 1 before the call).
//
//go:linkname __c2go_mktime
func __c2go_mktime(year, mon, mday, hour, min, sec int32, off *int64, isdst *int32, zone **byte) int64 {
	t := time.Date(int(year), time.Month(mon), int(mday), int(hour), int(min), int(sec), 0, time.Local)
	name, offset := t.Zone()
	*off = int64(offset)
	if t.IsDST() {
		*isdst = 1
	} else {
		*isdst = 0
	}
	*zone = internZone(name)
	return t.Unix()
}

// __c2go_tzset is the whole of tzset(). Go's time.Local is the ONE shared zone
// state — no private copy on this side. tzset synchronises it BIDIRECTIONALLY:
//
//   - C→Go: after C's setenv("TZ",...) (which flows through the same os
//     environment as process.go's Setenv), tzset re-reads $TZ and reassigns
//     time.Local, so Go's own time.Now().Local() now agrees with C.
//   - Go→C is automatic and needs nothing here: localtime/mktime read time.Local
//     LIVE on every call, so a Go-side `time.Local = x` is reflected immediately.
//     That is exactly why we must NOT cache the zone on our side — a cache would
//     go stale against Go's writes.
//
// When $TZ is unset we leave time.Local untouched (Go owns whatever it currently
// holds — its process-start system zone, or a value Go itself assigned); we do
// NOT force it back to a remembered default, which would clobber a Go-side change.
// tzset then snapshots the resolved zone into the POSIX tzname/timezone/daylight
// globals (std/dst names, std seconds WEST of UTC, DST-observed flag), picking std
// vs dst by sampling mid-winter/mid-summer so the southern hemisphere (January =
// DST) is handled too.
//
// reload != 0 means the full tzset() path: re-read $TZ into time.Local. reload
// == 0 is the lazy localtime/mktime path: only SNAPSHOT tzname/timezone/daylight
// from the CURRENT time.Local — it must never write time.Local, so a Go-side
// time.Local change is never clobbered by a mere localtime() call.
//
//go:linkname __c2go_tzset
func __c2go_tzset(reload int32, std **byte, dst **byte, stdWest *int64, hasdst *int32) {
	tzMu.Lock()
	if reload != 0 {
		if tz, ok := os.LookupEnv("TZ"); ok {
			// A leading ':' is the POSIX "implementation-defined" prefix; strip it.
			name := strings.TrimPrefix(tz, ":")
			switch {
			case name == "" || name == "UTC" || name == "UTC0":
				time.Local = time.UTC
			default:
				if loc, err := time.LoadLocation(name); err == nil {
					time.Local = loc
				}
				// An unparsable value (e.g. a bare POSIX "PST8PDT" spec, which
				// Go's LoadLocation does not parse) leaves time.Local unchanged —
				// best effort, matching how process.go defers env truth to os.
			}
		}
		// $TZ unset: leave time.Local as Go has it (see the doc comment).
	}
	loc := time.Local
	tzMu.Unlock()

	y := time.Now().Year()
	jan := time.Date(y, 1, 15, 12, 0, 0, 0, loc)
	jul := time.Date(y, 7, 15, 12, 0, 0, 0, loc)
	sName, sOff := jan.Zone()
	dName, dOff := jul.Zone()
	if jan.IsDST() { // southern hemisphere: January is the DST sample
		sName, sOff, dName, dOff = dName, dOff, sName, sOff
	}
	*std = internZone(sName)
	*stdWest = int64(-sOff)
	if sName != dName || sOff != dOff {
		*dst = internZone(dName)
		*hasdst = 1
	} else {
		*dst = internZone(sName)
		*hasdst = 0
	}
}

// __c2go_clock_gettime writes (sec, nsec) for the given c2go clock id. The out
// params point at C locals; passing real *int64 pointers is copystack-safe (the
// runtime adjusts interior stack pointers), unlike a fabricated-integer void*.
//go:linkname __c2go_clock_gettime
func __c2go_clock_gettime(id int32, sec *int64, nsec *int64) int32 {
	switch id {
	case 0: // CLOCK_REALTIME
		t := time.Now()
		*sec = t.Unix()
		*nsec = int64(t.Nanosecond())
	case 1, 2, 3: // CLOCK_MONOTONIC + CPUTIME (approximated by process-elapsed monotonic)
		d := time.Since(monoBase)
		*sec = int64(d / time.Second)
		*nsec = int64(d % time.Second)
	default:
		return errEINVAL
	}
	return 0
}

// __c2go_nanosleep parks the calling goroutine for sec+nsec. There is no early
// wakeup in this model, so nanosleep's remainder is always zero.
//go:linkname __c2go_nanosleep
func __c2go_nanosleep(sec int64, nsec int64) {
	if sec < 0 || nsec < 0 {
		return
	}
	time.Sleep(time.Duration(sec)*time.Second + time.Duration(nsec)*time.Nanosecond)
}
