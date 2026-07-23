#ifndef MEDIA_FABRIC_CORE_MEDIAFABRICRUNTIME_H
#define MEDIA_FABRIC_CORE_MEDIAFABRICRUNTIME_H

#include <memory>
#include <mutex>
#include <string>

namespace gb28181 {

class GB28181Node;
class ManagementServer;

// Owns the embeddable C++ runtime used by both the standalone diagnostic
// executable and the Go host. Business concerns deliberately stay outside
// this class; it only exposes existing node-management operations.
class MediaFabricRuntime
{
public:
    MediaFabricRuntime();
    ~MediaFabricRuntime();

    bool start(const std::string& configPath, bool enableManagementServer, std::string* error);
    void stop();
    bool running() const;
    std::string executeManagementCommand(const std::string& request);

private:
    MediaFabricRuntime(const MediaFabricRuntime&);
    MediaFabricRuntime& operator=(const MediaFabricRuntime&);

    mutable std::mutex m_mutex;
    std::unique_ptr<GB28181Node> m_node;
    std::unique_ptr<ManagementServer> m_managementServer;
    bool m_running;
};

} // namespace gb28181

#endif
