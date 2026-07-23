#ifndef MEDIA_FABRIC_CAPI_H
#define MEDIA_FABRIC_CAPI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct media_fabric_runtime media_fabric_runtime;

media_fabric_runtime* media_fabric_runtime_create(void);
void media_fabric_runtime_destroy(media_fabric_runtime* runtime);

// Starts the C++ media runtime. enable_management_socket keeps mfcli compatible
// while the Go HTTP API is introduced incrementally.
int media_fabric_runtime_start(media_fabric_runtime* runtime,
                               const char* config_path,
                               int enable_management_socket,
                               char* error_buffer,
                               size_t error_buffer_size);
void media_fabric_runtime_stop(media_fabric_runtime* runtime);
int media_fabric_runtime_running(const media_fabric_runtime* runtime);

// Sends an existing mfcli management command and writes its textual response.
int media_fabric_runtime_command(media_fabric_runtime* runtime,
                                 const char* command,
                                 char* response_buffer,
                                 size_t response_buffer_size);

#ifdef __cplusplus
}
#endif

#endif
