package media

/*
#cgo CFLAGS: -I${SRCDIR}/../../../src/capi
#cgo LDFLAGS: -lstdc++
#include <stdlib.h>
#include "media_fabric_c_api.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

const resultBufferSize = 64 * 1024

// Runtime is the Go-owned handle to the C++ GB28181 and media runtime.
// It intentionally exposes the existing management commands only; future Go
// business APIs should use typed functions instead of growing this command API.
type Runtime struct {
	handle *C.media_fabric_runtime
}

func NewRuntime() (*Runtime, error) {
	handle := C.media_fabric_runtime_create()
	if handle == nil {
		return nil, errors.New("cannot allocate media-fabric C++ runtime")
	}
	return &Runtime{handle: handle}, nil
}

func (r *Runtime) Start(configPath string, enableManagementSocket bool) error {
	if r == nil || r.handle == nil {
		return errors.New("media-fabric runtime is closed")
	}
	config := C.CString(configPath)
	defer C.free(unsafe.Pointer(config))
	errorBuffer := make([]C.char, resultBufferSize)
	enableManagement := C.int(0)
	if enableManagementSocket {
		enableManagement = 1
	}
	if C.media_fabric_runtime_start(r.handle,
		config,
		enableManagement,
		&errorBuffer[0],
		C.size_t(len(errorBuffer))) == 0 {
		return errors.New(C.GoString(&errorBuffer[0]))
	}
	return nil
}

func (r *Runtime) Stop() {
	if r != nil && r.handle != nil {
		C.media_fabric_runtime_stop(r.handle)
	}
}

func (r *Runtime) Close() {
	if r != nil && r.handle != nil {
		C.media_fabric_runtime_destroy(r.handle)
		r.handle = nil
	}
}

func (r *Runtime) Running() bool {
	return r != nil && r.handle != nil && C.media_fabric_runtime_running(r.handle) != 0
}

// Command preserves mfcli compatibility during migration. It is intentionally
// not the public Go business API surface.
func (r *Runtime) Command(command string) (string, error) {
	if r == nil || r.handle == nil {
		return "", errors.New("media-fabric runtime is closed")
	}
	request := C.CString(command)
	defer C.free(unsafe.Pointer(request))
	responseBuffer := make([]C.char, resultBufferSize)
	if C.media_fabric_runtime_command(r.handle,
		request,
		&responseBuffer[0],
		C.size_t(len(responseBuffer))) == 0 {
		return C.GoString(&responseBuffer[0]), errors.New("media-fabric command failed or response exceeds buffer")
	}
	return C.GoString(&responseBuffer[0]), nil
}
