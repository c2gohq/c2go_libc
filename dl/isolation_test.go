//go:build !windows

package dl

import (
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// TestIsolationFromToplevel scans every .go file in the c2go-libc
// top-level package and fails if any of them imports this package.
//
// c2go-libc top-level functions (Memcpy, Memcmp, Pthread*, …) are
// managed in-process implementations and must NEVER go through
// runtime.cgocall. Allowing them to depend on dl/ would let
// someone accidentally route a hot-path libc function through the
// g0 stack switch — defeating the layering described in this
// package's doc.go.
//
// Allowed importers of c2go-libc/dl:
//   - c2go-bind-generated wrappers in user packages
//   - end-user code wanting to call 3rd-party .so manually
func TestIsolationFromToplevel(t *testing.T) {
	parent, err := filepath.Abs("..")
	if err != nil {
		t.Fatal(err)
	}
	entries, err := os.ReadDir(parent)
	if err != nil {
		t.Fatal(err)
	}

	fset := token.NewFileSet()
	for _, e := range entries {
		if e.IsDir() || !strings.HasSuffix(e.Name(), ".go") {
			continue
		}
		path := filepath.Join(parent, e.Name())
		f, err := parser.ParseFile(fset, path, nil, parser.ImportsOnly)
		if err != nil {
			t.Errorf("parse %s: %v", e.Name(), err)
			continue
		}
		for _, imp := range f.Imports {
			ip := strings.Trim(imp.Path.Value, `"`)
			if strings.HasSuffix(ip, "/c2go_libc/dl") ||
				ip == "github.com/c2gohq/c2go_libc/dl" {
				t.Errorf(
					"%s imports %s — c2go-libc top-level must NOT depend on "+
						"dl/ (see dl/doc.go for rationale).",
					e.Name(), ip)
			}
		}
	}
}
