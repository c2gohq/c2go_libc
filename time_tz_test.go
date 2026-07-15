package libc

// Timezone-aware time tests (source/time.c + time.go). localtime/mktime are no
// longer the "== UTC" lie: they take the LOCAL zone offset from Go's time.Local,
// which is the single source of truth shared BIDIRECTIONALLY with the C side:
//   - C->Go: setenv("TZ",...) + tzset() reassigns time.Local.
//   - Go->C: localtime/mktime read time.Local live, so a Go-side time.Local
//     change is reflected with no cache to go stale.
// Plus strptime and the %z/%Z / tzname-globals surface.

import (
	"os"
	"testing"
	"time"
	"unsafe" // pointer casts + //go:linkname
)

// The C globals refreshed by tzset(), reached the same way getopt_test.go reaches
// optarg: //go:linkname onto the lowercase C symbol.
//
//go:linkname goTzname github.com/c2gohq/c2go_libc.tzname
var goTzname [2]*byte

//go:linkname goTimezone github.com/c2gohq/c2go_libc.timezone
var goTimezone int64

//go:linkname goDaylight github.com/c2gohq/c2go_libc.daylight
var goDaylight int32

// saveTZ restores $TZ and time.Local after the test, so mutating the process-wide
// zone here cannot leak into other tests in the package.
func saveTZ(t *testing.T) {
	t.Helper()
	old, had := os.LookupEnv("TZ")
	savedLocal := time.Local
	t.Cleanup(func() {
		if had {
			os.Setenv("TZ", old)
		} else {
			os.Unsetenv("TZ")
		}
		time.Local = savedLocal
	})
}

// setLibcTZ drives the zone through the C world: the libc Setenv (-> os.Setenv)
// then tzset(), exactly as a C program would. Skips if tzdata is unavailable.
func setLibcTZ(t *testing.T, zone string) *time.Location {
	t.Helper()
	loc, err := time.LoadLocation(zone)
	if err != nil {
		t.Skipf("no tzdata for %q: %v", zone, err)
	}
	Setenv(csb("TZ"), csb(zone), 1)
	Tzset()
	// C->Go: tzset must have made Go's own time.Local agree.
	if time.Local.String() != loc.String() {
		t.Fatalf("tzset did not sync time.Local: got %q, want %q", time.Local, loc)
	}
	return loc
}

func TestLocaltimeReal(t *testing.T) {
	saveTZ(t)
	loc := setLibcTZ(t, "America/New_York")

	// A summer instant (EDT, -04:00) and a winter instant (EST, -05:00).
	for _, u := range []int64{1688212800 /*2023-07-01 12:00 UTC*/, 1704110400 /*2024-01-01 12:00 UTC*/} {
		sec := u
		got := (*tmMirror)(unsafe.Pointer(Localtime(&sec)))
		want := time.Unix(u, 0).In(loc)
		nameWant, offWant := want.Zone()

		if int(got.year)+1900 != want.Year() || int(got.mon)+1 != int(want.Month()) ||
			int(got.mday) != want.Day() || int(got.hour) != want.Hour() ||
			int(got.min) != want.Minute() || int(got.sec) != want.Second() {
			t.Errorf("localtime(%d) = %04d-%02d-%02d %02d:%02d:%02d, want %v",
				u, int(got.year)+1900, got.mon+1, got.mday, got.hour, got.min, got.sec, want)
		}
		if int(got.gmtoff) != offWant {
			t.Errorf("localtime(%d) gmtoff = %d, want %d", u, got.gmtoff, offWant)
		}
		if got.gmtoff == 0 {
			t.Errorf("localtime(%d) still has a zero offset — the UTC lie is back", u)
		}
		wantDST := int32(0)
		if want.IsDST() {
			wantDST = 1
		}
		if got.isdst != wantDST {
			t.Errorf("localtime(%d) isdst = %d, want %d", u, got.isdst, wantDST)
		}
		// Read tm_zone through strftime %Z (C-side deref) rather than the mirror's
		// uintptr field, to avoid a uintptr->Pointer vet flag.
		if z := strftimeStr("%Z", got); z != nameWant {
			t.Errorf("localtime(%d) zone = %q, want %q", u, z, nameWant)
		}
	}
}

func TestLocaltimeGoToC(t *testing.T) {
	saveTZ(t)
	// Go changes the zone directly (no tzset); C localtime must reflect it live,
	// because it reads time.Local per call rather than caching.
	loc, err := time.LoadLocation("Asia/Kolkata") // +05:30, no DST
	if err != nil {
		t.Skipf("no tzdata: %v", err)
	}
	time.Local = loc

	var u int64 = 1688212800
	got := (*tmMirror)(unsafe.Pointer(Localtime(&u)))
	if got.gmtoff != 19800 { // +5:30
		t.Errorf("Go->C: localtime gmtoff = %d, want 19800 (IST)", got.gmtoff)
	}
	want := time.Unix(u, 0).In(loc)
	if int(got.hour) != want.Hour() || int(got.min) != want.Minute() {
		t.Errorf("Go->C: localtime = %02d:%02d, want %02d:%02d", got.hour, got.min, want.Hour(), want.Minute())
	}
}

func TestMktimeLocal(t *testing.T) {
	saveTZ(t)
	loc := setLibcTZ(t, "America/New_York")

	// A local wall-clock time in summer -> the same instant Go's time.Date gives.
	m := tmMirror{year: 2023 - 1900, mon: 7 - 1, mday: 1, hour: 8, min: 0, sec: 0, isdst: -1}
	got := Mktime(asTm(&m))
	want := time.Date(2023, 7, 1, 8, 0, 0, 0, loc).Unix()
	if int64(got) != want {
		t.Errorf("mktime(local NY) = %d, want %d", got, want)
	}
	// mktime refills the offset/zone from the resolved instant (EDT = -4h).
	if m.gmtoff != -4*3600 {
		t.Errorf("mktime gmtoff = %d, want %d (EDT)", m.gmtoff, -4*3600)
	}
	// Normalisation of out-of-range fields is still zone-correct (Jan 32 -> Feb 1).
	n := tmMirror{year: 2023 - 1900, mon: 0, mday: 32, isdst: -1}
	Mktime(asTm(&n))
	if n.mon != 1 || n.mday != 1 {
		t.Errorf("mktime(Jan 32) -> mon=%d mday=%d, want 1/1", n.mon, n.mday)
	}
}

func TestTzsetGlobals(t *testing.T) {
	saveTZ(t)
	setLibcTZ(t, "America/New_York")

	if goTimezone != 5*3600 { // std (EST) is 5h WEST of UTC
		t.Errorf("timezone = %d, want 18000 (EST west of UTC)", goTimezone)
	}
	if goDaylight != 1 {
		t.Errorf("daylight = %d, want 1 (NY observes DST)", goDaylight)
	}
	if n := cstr(goTzname[0]); n != "EST" {
		t.Errorf("tzname[0] = %q, want EST", n)
	}
	if n := cstr(goTzname[1]); n != "EDT" {
		t.Errorf("tzname[1] = %q, want EDT", n)
	}

	// A no-DST zone: daylight 0, both tznames equal.
	setLibcTZ(t, "Asia/Kolkata")
	if goDaylight != 0 {
		t.Errorf("daylight = %d for Kolkata, want 0", goDaylight)
	}
	if goTimezone != -(5*3600 + 30*60) { // +5:30 east -> negative west
		t.Errorf("timezone = %d for Kolkata, want %d", goTimezone, -(5*3600 + 30*60))
	}
}

func TestStrftimeZoneLocal(t *testing.T) {
	saveTZ(t)
	setLibcTZ(t, "America/New_York")
	var u int64 = 1688212800 // summer -> EDT
	got := (*tmMirror)(unsafe.Pointer(Localtime(&u)))
	if s := strftimeStr("%z %Z", got); s != "-0400 EDT" {
		t.Errorf("strftime(%%z %%Z) = %q, want -0400 EDT", s)
	}
}

func TestStrptime(t *testing.T) {
	// Full numeric date-time.
	var m tmMirror
	if Strptime(csb("2023-07-01 13:45:30"), csb("%Y-%m-%d %H:%M:%S"), asTm(&m)) == nil {
		t.Fatal("strptime returned NULL")
	}
	if int(m.year)+1900 != 2023 || int(m.mon)+1 != 7 || m.mday != 1 ||
		m.hour != 13 || m.min != 45 || m.sec != 30 {
		t.Fatalf("strptime numeric = %+v", m)
	}

	// Symbolic full names + weekday.
	var m2 tmMirror
	if Strptime(csb("Sunday, July 02, 2023"), csb("%A, %B %d, %Y"), asTm(&m2)) == nil {
		t.Fatal("strptime symbolic returned NULL")
	}
	if m2.wday != 0 || int(m2.mon)+1 != 7 || m2.mday != 2 || int(m2.year)+1900 != 2023 {
		t.Fatalf("strptime symbolic = %+v", m2)
	}

	// Abbreviated month + 12-hour clock with PM.
	var m3 tmMirror
	if Strptime(csb("Jan 05 01:30 PM"), csb("%b %d %I:%M %p"), asTm(&m3)) == nil {
		t.Fatal("strptime ampm returned NULL")
	}
	if int(m3.mon)+1 != 1 || m3.mday != 5 || m3.hour != 13 || m3.min != 30 {
		t.Fatalf("strptime ampm = %+v", m3)
	}

	// %z offset parsing.
	var m4 tmMirror
	if Strptime(csb("+0530"), csb("%z"), asTm(&m4)) == nil {
		t.Fatal("strptime %z returned NULL")
	}
	if m4.gmtoff != 5*3600+30*60 {
		t.Errorf("strptime %%z gmtoff = %d, want %d", m4.gmtoff, 5*3600+30*60)
	}

	// A bad input returns NULL.
	var m5 tmMirror
	if Strptime(csb("not-a-date"), csb("%Y-%m-%d"), asTm(&m5)) != nil {
		t.Error("strptime(bad) should return NULL")
	}

	// Round-trip: strftime a UTC instant, strptime it back.
	var src tmMirror
	s0 := int64(1700000000)
	GmtimeR(&s0, asTm(&src))
	formatted := strftimeStr("%Y-%m-%d %H:%M:%S", &src)
	var back tmMirror
	if Strptime(csb(formatted), csb("%Y-%m-%d %H:%M:%S"), asTm(&back)) == nil {
		t.Fatalf("strptime round-trip of %q returned NULL", formatted)
	}
	if back.year != src.year || back.mon != src.mon || back.mday != src.mday ||
		back.hour != src.hour || back.min != src.min || back.sec != src.sec {
		t.Fatalf("strptime round-trip: got %+v, want %+v", back, src)
	}
}
