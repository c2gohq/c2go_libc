//go:build go1.27 && c2go_allow_unverified_go_toolchain

package c2goabi

// GoToolchainContractEpoch is an explicit test-only assumption for a Go
// release that this c2go-libc version has not validated. Release builds and CI
// support claims must never set c2go_allow_unverified_go_toolchain.
const GoToolchainContractEpoch = 1
