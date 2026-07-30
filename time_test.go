package libc

import (
	"testing"
	"time"
	"unsafe"
)

// mirror the C struct layouts (both fields int64, per bits/alltypes.h). The
// generated timespec/timeval bindings are opaque struct{}, so tests back them
// with these and cast the pointer.
type tsMirror struct{ sec, nsec int64 }
type tvMirror struct{ sec, usec int64 }

// tmMirror mirrors `struct tm`: nine int32 fields, target-width C long
// tm_gmtoff, then const char *tm_zone. testLong makes both LP64 and Windows
// LLP64 use the same offsets as the generated C layout.
type tmMirror struct {
	sec, min, hour, mday, mon, year int32
	wday, yday, isdst               int32
	gmtoff                          testLong
	zone                            uintptr
}

// asTm casts a *tmMirror to the opaque *tm binding type.
func asTm(m *tmMirror) *tm { return (*tm)(unsafe.Pointer(m)) }

// TestCalendar cross-checks gmtime/mktime/asctime/ctime against Go's time pkg.
func TestCalendar(t *testing.T) {
	for _, epoch := range []int64{0, 1700000000, -1, 951782400, 1583020800, 253402300799} {
		sec := epoch
		var cur tmMirror
		if GmtimeR(&sec, asTm(&cur)) == nil {
			t.Fatalf("gmtime_r(%d) returned nil", epoch)
		}
		want := time.Unix(epoch, 0).UTC()
		if int(cur.year)+1900 != want.Year() || int(cur.mon) != int(want.Month())-1 ||
			int(cur.mday) != want.Day() || int(cur.hour) != want.Hour() ||
			int(cur.min) != want.Minute() || int(cur.sec) != want.Second() ||
			int(cur.wday) != int(want.Weekday()) || int(cur.yday) != want.YearDay()-1 {
			t.Fatalf("gmtime_r(%d) = %+v, want %v", epoch, cur, want)
		}
		// timegm (the UTC inverse of gmtime) must round-trip the UTC tm back to
		// the epoch. mktime would instead apply the LOCAL zone offset — that
		// direction is covered in time_tz_test.go.
		if got := Timegm(asTm(&cur)); got != epoch {
			t.Fatalf("timegm round-trip(%d) = %d", epoch, got)
		}
	}

	// mktime normalises out-of-range fields (2023 Feb 30 -> Mar 2).
	feb30 := tmMirror{year: 123, mon: 1, mday: 30}
	Mktime(asTm(&feb30))
	if feb30.mon != 2 || feb30.mday != 2 {
		t.Fatalf("mktime(Feb30) -> mon=%d mday=%d, want 2/2", feb30.mon, feb30.mday)
	}

	// asctime / ctime formatting for the epoch.
	var t0 int64
	var z tmMirror
	GmtimeR(&t0, asTm(&z))
	buf := make([]byte, 26)
	if got := cstr(AsctimeR(asTm(&z), &buf[0])); got != "Thu Jan  1 00:00:00 1970\n" {
		t.Fatalf("asctime = %q", got)
	}
	// ctime is LOCAL (asctime(localtime(t))), so oracle it against Go's own
	// time.Local for the same instant — deterministic on any host zone, and no
	// longer the "localtime == UTC" assumption.
	wantCtime := time.Unix(0, 0).Local().Format(time.ANSIC) + "\n"
	if got := cstr(Ctime(&t0)); got != wantCtime {
		t.Fatalf("ctime = %q, want %q", got, wantCtime)
	}
}

func TestTime(t *testing.T) {
	// time() ~ Go's wall clock (within a couple of seconds).
	now := time.Now().Unix()
	if got := Time(nil); got < now-2 || got > now+2 {
		t.Fatalf("time() = %d, want ~%d", got, now)
	}
	var tt int64
	if got := Time(&tt); got != tt {
		t.Fatalf("time(&t): return %d != *t %d", got, tt)
	}

	// clock_gettime(CLOCK_REALTIME)
	var ts tsMirror
	if ClockGettime(0, (*timespec)(unsafe.Pointer(&ts))) != 0 {
		t.Fatal("clock_gettime(REALTIME) failed")
	}
	if ts.sec < now-2 || ts.sec > now+2 || ts.nsec < 0 || ts.nsec >= 1e9 {
		t.Fatalf("REALTIME ts = {%d,%d}", ts.sec, ts.nsec)
	}

	// clock_gettime(CLOCK_MONOTONIC) is non-decreasing.
	var m1, m2 tsMirror
	ClockGettime(1, (*timespec)(unsafe.Pointer(&m1)))
	time.Sleep(2 * time.Millisecond)
	ClockGettime(1, (*timespec)(unsafe.Pointer(&m2)))
	ns1 := m1.sec*1e9 + m1.nsec
	ns2 := m2.sec*1e9 + m2.nsec
	if ns2 < ns1 {
		t.Fatalf("MONOTONIC went backwards: %d -> %d", ns1, ns2)
	}

	// bad clock id -> EINVAL (-1)
	if ClockGettime(99, (*timespec)(unsafe.Pointer(&ts))) != -1 {
		t.Fatal("clock_gettime(bad id) should return -1")
	}

	// gettimeofday
	var tv tvMirror
	if Gettimeofday((*timeval)(unsafe.Pointer(&tv)), nil) != 0 {
		t.Fatal("gettimeofday failed")
	}
	if tv.sec < now-2 || tv.usec < 0 || tv.usec >= 1e6 {
		t.Fatalf("gettimeofday tv = {%d,%d}", tv.sec, tv.usec)
	}

	// clock_getres -> 1ns
	var res tsMirror
	ClockGetres(0, (*timespec)(unsafe.Pointer(&res)))
	if res.sec != 0 || res.nsec != 1 {
		t.Fatalf("clock_getres = {%d,%d}, want {0,1}", res.sec, res.nsec)
	}

	// timespec_get(TIME_UTC) returns TIME_UTC(1)
	if TimespecGet((*timespec)(unsafe.Pointer(&ts)), 1) != 1 {
		t.Fatal("timespec_get(TIME_UTC) should return 1")
	}

	// difftime
	if Difftime(100, 40) != 60.0 {
		t.Fatalf("difftime(100,40) = %v, want 60", Difftime(100, 40))
	}

	// clock() advances
	c1 := Clock()
	time.Sleep(2 * time.Millisecond)
	if Clock() < c1 {
		t.Fatal("clock() went backwards")
	}
}

func strftimeStr(format string, m *tmMirror) string {
	fbuf := append([]byte(format), 0)
	out := make([]byte, 256)
	n := Strftime(&out[0], uint64(len(out)), &fbuf[0], asTm(m))
	return string(out[:n])
}

// TestStrftime checks the common conversion specifiers, cross-checking the
// numeric/date ones against Go's time formatting.
func TestStrftime(t *testing.T) {
	var t0 int64
	var z tmMirror
	GmtimeR(&t0, asTm(&z)) // 1970-01-01 00:00:00 Thu

	cases := []struct{ f, want string }{
		{"%Y-%m-%d", "1970-01-01"},
		{"%H:%M:%S", "00:00:00"},
		{"%A, %B %d, %Y", "Thursday, January 01, 1970"},
		{"%a %b %e", "Thu Jan  1"},
		{"%p %I:%M", "AM 12:00"},
		{"%j", "001"},
		{"%F %T", "1970-01-01 00:00:00"},
		{"100%%done", "100%done"},
		{"%z %Z", "+0000 UTC"},
		{"%w %u", "4 4"},
	}
	for _, c := range cases {
		if got := strftimeStr(c.f, &z); got != c.want {
			t.Fatalf("strftime(%q) = %q, want %q", c.f, got, c.want)
		}
	}

	// cross-check %F %T against Go for several epochs.
	for _, epoch := range []int64{1700000000, 951782400, 1583020800} {
		sec := epoch
		var cur tmMirror
		GmtimeR(&sec, asTm(&cur))
		want := time.Unix(epoch, 0).UTC().Format("2006-01-02 15:04:05")
		if got := strftimeStr("%F %T", &cur); got != want {
			t.Fatalf("strftime(%d) = %q, want %q", epoch, got, want)
		}
	}

	// truncation: too-small buffer returns 0.
	fbuf := append([]byte("%Y-%m-%d"), 0)
	small := make([]byte, 4)
	if n := Strftime(&small[0], uint64(len(small)), &fbuf[0], asTm(&z)); n != 0 {
		t.Fatalf("strftime into tiny buffer returned %d, want 0", n)
	}
}

// TestNanosleep confirms nanosleep actually parks for ~the requested duration.
func TestNanosleep(t *testing.T) {
	req := tsMirror{sec: 0, nsec: 20 * 1e6} // 20ms
	start := time.Now()
	if Nanosleep((*timespec)(unsafe.Pointer(&req)), nil) != 0 {
		t.Fatal("nanosleep failed")
	}
	if el := time.Since(start); el < 10*time.Millisecond {
		t.Fatalf("nanosleep(20ms) only slept %v", el)
	}
	// negative nsec -> EINVAL
	bad := tsMirror{sec: 0, nsec: -1}
	if Nanosleep((*timespec)(unsafe.Pointer(&bad)), nil) != -1 {
		t.Fatal("nanosleep(bad) should return -1")
	}
}
