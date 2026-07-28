//go:build darwin || freebsd || linux || netbsd

package dl

import "github.com/ebitengine/purego"

// Dlopen opens a shared library and returns its host loader handle.
func Dlopen(path string, mode int) (uintptr, error) {
	return purego.Dlopen(path, mode)
}

// Dlsym resolves name in handle. RTLD_DEFAULT searches the process-global
// dynamic-loader namespace.
func Dlsym(handle uintptr, name string) (uintptr, error) {
	return purego.Dlsym(handle, name)
}

// Dlclose releases a shared-library handle.
func Dlclose(handle uintptr) error {
	return purego.Dlclose(handle)
}

// Dynamic-loader constants supplied by PureGo for the target Unix platform.
const (
	RTLD_DEFAULT = purego.RTLD_DEFAULT
	RTLD_LAZY    = purego.RTLD_LAZY
	RTLD_NOW     = purego.RTLD_NOW
	RTLD_LOCAL   = purego.RTLD_LOCAL
	RTLD_GLOBAL  = purego.RTLD_GLOBAL
)
