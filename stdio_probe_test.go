package libc

// #585 regression: snprintf builds its sink FILE (plus musl's transient
// 80-byte internal_buf) ON THE STACK, and the deep printf_core chain grows the
// goroutine stack mid-format. Before the C2GoFrameAddrRemat root fix, a
// register-allocator spill of a materialized frame address survived copystack
// unrelocated and vfprintf's tail read a dead pre-copy FILE image, yielding
// all-zero output. Driving the call from many C-recursion depths shifts where
// morestack fires inside the chain, sweeping the vulnerable suspension points.

import "testing"

func TestSnprintfDepthSweep(t *testing.T) {
	for depth := 0; depth < 400; depth++ {
		buf := make([]byte, 32)
		ret := SnprintfProbe(&buf[0], 32, int32(depth))
		if ret != 2 || string(buf[:2]) != "42" {
			t.Fatalf("depth=%d: ret=%d got=%q (stack FILE broke under copystack)",
				depth, ret, buf[:2])
		}
	}
}
