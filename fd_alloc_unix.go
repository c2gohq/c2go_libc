//go:build unix

package libc

import "golang.org/x/sys/unix"

// c2goFD moves a newly-created C-facing descriptor out of 0/1/2. Those three
// numbers are reserved by c2go for the live Go standard streams, even when the
// corresponding kernel descriptor is temporarily closed. Without this step,
// open/pipe/dup can reuse (for example) fd 0 and later read/write/close calls
// incorrectly take the virtual-stdin path.
//
// The helper consumes fd when it is in the reserved range, on both success and
// failure. F_DUPFD atomically chooses the lowest free descriptor >= 3; preserve
// FD_CLOEXEC because F_DUPFD itself clears it.
func c2goFD(fd int) (int, error) {
	if fd >= 3 {
		return fd, nil
	}

	flags, err := unix.FcntlInt(uintptr(fd), unix.F_GETFD, 0)
	if err != nil {
		_ = unix.Close(fd)
		return -1, err
	}
	nfd, err := unix.FcntlInt(uintptr(fd), unix.F_DUPFD, 3)
	if err != nil {
		_ = unix.Close(fd)
		return -1, err
	}
	if flags&unix.FD_CLOEXEC != 0 {
		if _, err = unix.FcntlInt(uintptr(nfd), unix.F_SETFD, flags); err != nil {
			_ = unix.Close(nfd)
			_ = unix.Close(fd)
			return -1, err
		}
	}
	// Like the close shim, do not retry: Linux may already have released fd on
	// EINTR, and retrying could close an unrelated descriptor after reuse.
	_ = unix.Close(fd)
	return nfd, nil
}

func c2goFDPair(fds *[2]int) error {
	first, err := c2goFD(fds[0])
	if err != nil {
		_ = unix.Close(fds[1])
		return err
	}
	second, err := c2goFD(fds[1])
	if err != nil {
		_ = unix.Close(first)
		return err
	}
	fds[0], fds[1] = first, second
	return nil
}

// c2goFcntlResult applies the same reservation rule to the two fcntl commands
// that create descriptors. Other fcntl results are ordinary integers.
func c2goFcntlResult(cmd, result int32) int32 {
	if result < 0 || (cmd != unix.F_DUPFD && cmd != unix.F_DUPFD_CLOEXEC) {
		return result
	}
	fd, err := c2goFD(int(result))
	if err != nil {
		return -errnoOf(err)
	}
	return int32(fd)
}
