// Nested module: drops csrc/ (our C + annotated headers = build inputs only) out of the
// parent module's proxy zip. No Go code ships from here.
module github.com/c2gohq/c2go_libc/csrc

go 1.25.0
