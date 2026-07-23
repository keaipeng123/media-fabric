#include "media_fabric_c_api.h"

#include <cstring>

int main()
{
    media_fabric_runtime* runtime = media_fabric_runtime_create();
    if (runtime == 0 || media_fabric_runtime_running(runtime) != 0)
    {
        media_fabric_runtime_destroy(runtime);
        return 1;
    }

    char error[128];
    std::memset(error, 0, sizeof(error));
    const int started = media_fabric_runtime_start(runtime, "", 0, error, sizeof(error));
    const bool rejectedInvalidStart = started == 0 && std::strlen(error) > 0;
    media_fabric_runtime_stop(runtime);
    media_fabric_runtime_destroy(runtime);
    return rejectedInvalidStart ? 0 : 1;
}
