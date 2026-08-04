//go:build go1.25 && !go1.27

package c2goabi

// GoToolchainContractEpoch is the contract generation validated for the active
// Go assembler, linker, runtime, ABI0 frame rules, and GC metadata semantics.
//
// Go 1.25 and Go 1.26 share epoch 1. Extending a compatible upper bound changes
// only this provider; generated schema-v2 packages do not need regeneration.
const GoToolchainContractEpoch = 1
