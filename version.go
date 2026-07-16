package libc

// C2GoABIEpoch — the c2go-internal ABI generation (calling conventions,
// symbol / gcbitmap naming, manifest schema, managed model) this build of
// c2go-libc speaks. A c2go-clang-generated consumer package must emit an ABI
// epoch within [C2GoABIEpochMin, C2GoABIEpochMax]; the c2go-bind-generated
// c2go_abi_anchor.go enforces that at `go build` time via a const static
// assert (see c2go-bind main.go emitABIAnchor + docs/c2go/versioning.md).
// Bump only on an intentional ABI break.
const (
	C2GoABIEpochMin = 1
	C2GoABIEpochMax = 1
)
