// SPDX-License-Identifier: AGPL-3.0-only

package namespaced

import (
	"io"
	"os"
	"testing"

	libc "github.com/c2gohq/c2go_libc"
)

func TestManagedStdioHeader(t *testing.T) {
	if got := MlibStdioPrefixedSelftest(); got != 0 {
		t.Fatalf("C namespaced stdio selftest = %d", got)
	}
}

func TestManagedStandardStreams(t *testing.T) {
	inputReader, inputWriter, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	outputReader, outputWriter, err := os.Pipe()
	if err != nil {
		inputReader.Close()
		inputWriter.Close()
		t.Fatal(err)
	}
	oldStdin, oldStdout := os.Stdin, os.Stdout
	os.Stdin, os.Stdout = inputReader, outputWriter
	t.Cleanup(func() {
		os.Stdin, os.Stdout = oldStdin, oldStdout
		inputReader.Close()
		inputWriter.Close()
		outputReader.Close()
		outputWriter.Close()
	})

	if _, err := inputWriter.Write([]byte{'Q'}); err != nil {
		t.Fatal(err)
	}
	if err := inputWriter.Close(); err != nil {
		t.Fatal(err)
	}
	if got := MlibStdioPrefixedStdoutSelftest(); got != 0 {
		t.Fatalf("C namespaced standard-output selftest = %d", got)
	}
	if got := MlibStdioPrefixedStdinSelftest(); got != 'Q' {
		t.Fatalf("C namespaced getchar = %q, want Q", got)
	}

	libc.C2goFinalize()
	if err := outputWriter.Close(); err != nil {
		t.Fatal(err)
	}
	os.Stdout = oldStdout
	output, err := io.ReadAll(outputReader)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := string(output), "managed-stdio:42|line\n!"; got != want {
		t.Fatalf("managed stdout = %q, want %q", got, want)
	}
}
