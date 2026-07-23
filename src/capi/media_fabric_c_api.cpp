#include "media_fabric_c_api.h"

#include "MediaFabricRuntime.h"

#include <cstring>
#include <memory>
#include <string>

struct media_fabric_runtime
{
    gb28181::MediaFabricRuntime runtime;
};

namespace {

void copyText(const std::string& text, char* buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }
    const size_t copySize = text.size() < bufferSize - 1 ? text.size() : bufferSize - 1;
    if (copySize > 0)
    {
        std::memcpy(buffer, text.data(), copySize);
    }
    buffer[copySize] = '\0';
}

} // namespace

extern "C" media_fabric_runtime* media_fabric_runtime_create(void)
{
    return new media_fabric_runtime();
}

extern "C" void media_fabric_runtime_destroy(media_fabric_runtime* runtime)
{
    delete runtime;
}

extern "C" int media_fabric_runtime_start(media_fabric_runtime* runtime,
                                            const char* configPath,
                                            int enableManagementSocket,
                                            char* errorBuffer,
                                            size_t errorBufferSize)
{
    if (runtime == NULL || configPath == NULL || configPath[0] == '\0')
    {
        copyText("invalid media-fabric runtime start arguments", errorBuffer, errorBufferSize);
        return 0;
    }
    std::string error;
    if (!runtime->runtime.start(configPath, enableManagementSocket != 0, &error))
    {
        copyText(error, errorBuffer, errorBufferSize);
        return 0;
    }
    copyText("", errorBuffer, errorBufferSize);
    return 1;
}

extern "C" void media_fabric_runtime_stop(media_fabric_runtime* runtime)
{
    if (runtime != NULL)
    {
        runtime->runtime.stop();
    }
}

extern "C" int media_fabric_runtime_running(const media_fabric_runtime* runtime)
{
    return runtime != NULL && runtime->runtime.running() ? 1 : 0;
}

extern "C" int media_fabric_runtime_command(media_fabric_runtime* runtime,
                                              const char* command,
                                              char* responseBuffer,
                                              size_t responseBufferSize)
{
    if (runtime == NULL || command == NULL || responseBuffer == NULL || responseBufferSize == 0)
    {
        return 0;
    }
    const std::string response = runtime->runtime.executeManagementCommand(command);
    copyText(response, responseBuffer, responseBufferSize);
    return response.size() < responseBufferSize ? 1 : 0;
}
