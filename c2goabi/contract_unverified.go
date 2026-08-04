//go:build go1.27 && !c2go_allow_unverified_go_toolchain

package c2goabi

// Keep the state machine explicit for tooling, then fail this provider package
// itself so libc-only builds and consumers without a generated anchor cannot
// accidentally treat an unknown Go release as compatible.
const GoToolchainContractEpoch = -1

var _ = C2GO_LIBC_HAS_NOT_VALIDATED_THIS_GO_TOOLCHAIN__UPGRADE_C2GO_LIBC
