//go:build windows

package dl_test

import (
	"math"
	"testing"

	"github.com/c2gohq/c2go_libc/dl"
)

func TestCallWindowsInteger(t *testing.T) {
	fn, err := dl.Dlsym(dl.RTLD_DEFAULT, "GetCurrentProcessId")
	if err != nil {
		t.Fatal(err)
	}
	got, _ := dl.Call(fn, nil, 0, dl.RInt, nil, nil)
	if got == 0 {
		t.Fatal("GetCurrentProcessId returned zero")
	}
}

func TestCallWindowsFloat(t *testing.T) {
	h, err := dl.Dlopen("msvcrt.dll", dl.RTLD_LAZY)
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := dl.Dlclose(h); err != nil {
			t.Errorf("Dlclose: %v", err)
		}
	}()

	fn, err := dl.Dlsym(h, "sqrt")
	if err != nil {
		t.Fatal(err)
	}
	_, bits := dl.Call(fn,
		[]dl.Piece{{Class: dl.PFloat, Val: uintptr(math.Float64bits(81))}},
		0, dl.RFloat, nil, nil)
	if got := math.Float64frombits(uint64(bits)); got != 9 {
		t.Fatalf("sqrt(81) = %v, want 9", got)
	}
}
