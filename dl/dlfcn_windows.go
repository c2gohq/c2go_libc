//go:build windows

package dl

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// These constants keep the portable dl API source-compatible on Windows.
// LoadLibrary does not expose Unix lazy/global binding modes, so Dlopen ignores
// every mode bit. RTLD_DEFAULT is represented by the zero pseudo-handle.
const (
	RTLD_DEFAULT = 0
	RTLD_LAZY    = 1
	RTLD_NOW     = 2
	RTLD_LOCAL   = 0
	RTLD_GLOBAL  = 0x100
)

// Dlopen loads a DLL. mode is accepted for source compatibility with Unix.
func Dlopen(path string, mode int) (uintptr, error) {
	_ = mode
	h, err := windows.LoadLibrary(path)
	return uintptr(h), err
}

// Dlsym resolves name in a DLL. For RTLD_DEFAULT it searches every module
// currently loaded in this process, approximating the Unix global namespace.
func Dlsym(handle uintptr, name string) (uintptr, error) {
	if handle != RTLD_DEFAULT {
		return windows.GetProcAddress(windows.Handle(handle), name)
	}
	return findProcessSymbol(name)
}

// Dlclose releases a DLL handle returned by Dlopen.
func Dlclose(handle uintptr) error {
	if handle == RTLD_DEFAULT {
		return fmt.Errorf("c2go/dl: cannot close RTLD_DEFAULT")
	}
	return windows.FreeLibrary(windows.Handle(handle))
}

func findProcessSymbol(name string) (uintptr, error) {
	const initialModules = 64
	handleSize := uint32(unsafe.Sizeof(windows.Handle(0)))
	modules := make([]windows.Handle, initialModules)

	for {
		var needed uint32
		err := windows.EnumProcessModules(
			windows.CurrentProcess(),
			&modules[0],
			uint32(len(modules))*handleSize,
			&needed,
		)
		if err != nil {
			return 0, fmt.Errorf("c2go/dl: enumerate process modules: %w", err)
		}
		count := int(needed / handleSize)
		if count <= len(modules) {
			modules = modules[:count]
			break
		}
		modules = make([]windows.Handle, count)
	}

	for _, module := range modules {
		if fn, err := windows.GetProcAddress(module, name); err == nil && fn != 0 {
			return fn, nil
		}
	}
	return 0, fmt.Errorf("c2go/dl: symbol %q not found in loaded modules", name)
}
