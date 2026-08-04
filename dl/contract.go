package dl

import "github.com/c2gohq/c2go_libc/c2goabi"

// dl's callback trampolines are hand-written Go ABI0 assembly, so they share
// the same Go-toolchain contract gate as clang-generated packages.
const requiredGoToolchainContractEpoch = 1

const (
	_ = uint(c2goabi.GoToolchainContractEpoch - requiredGoToolchainContractEpoch)
	_ = uint(requiredGoToolchainContractEpoch - c2goabi.GoToolchainContractEpoch)
)
