#include "MediaFabricRuntime.h"

#include "GB28181Node.h"
#include "Logger.h"
#include "ManagementServer.h"
#include "NodeConfig.h"
#include "StandardCapabilities.h"

#include <sstream>
#include <utility>
#include <vector>

namespace gb28181 {
namespace {

std::string executeCommand(GB28181Node& node, const std::string& request)
{
    std::istringstream input(request);
    std::string command;
    std::string peerId;
    input >> command >> peerId;
    if (command == "peers")
    {
        return std::string("OK\n") + node.peersStatusText();
    }

    std::string error;
    if (command == "register")
    {
        return node.requestRegistration(peerId, &error) ? "OK REGISTER sent\n" : "ERROR " + error + "\n";
    }
    if (command == "invite")
    {
        return node.requestInvite(peerId, &error) ? "OK INVITE sent\n" : "ERROR " + error + "\n";
    }
    if (command == "bye")
    {
        return node.requestBye(peerId, &error) ? "OK BYE sent\n" : "ERROR " + error + "\n";
    }
    if (command == "streams")
    {
        return std::string("OK\n") + node.streamsStatusText();
    }
    if (command == "catalog-query")
    {
        return node.requestCatalog(peerId, &error) ? "OK Catalog query sent\n" : "ERROR " + error + "\n";
    }
    if (command == "catalog-show")
    {
        return std::string("OK\n") + node.catalogJson(peerId) + "\n";
    }
    if (command == "help")
    {
        return "OK commands: peers, register <peer-id>, invite <catalog-device-id>, bye <catalog-device-id>, streams, catalog-query <peer-id>, catalog-show <peer-id>\n";
    }
    return "ERROR unknown command\n";
}

} // namespace

MediaFabricRuntime::MediaFabricRuntime() : m_running(false) {}

MediaFabricRuntime::~MediaFabricRuntime()
{
    stop();
}

bool MediaFabricRuntime::start(const std::string& configPath,
                               bool enableManagementServer,
                               std::string* error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running)
    {
        if (error)
        {
            *error = "media-fabric runtime is already running";
        }
        return false;
    }

    NodeConfig config;
    if (!config.load(configPath))
    {
        if (error)
        {
            *error = "failed to load config: " + configPath;
        }
        return false;
    }

    media_fabric::Logger::instance().configure(media_fabric::LOG_INFO,
                                                 "media-fabric.log",
                                                 10 * 1024 * 1024,
                                                 5);
    std::unique_ptr<GB28181Node> node(new GB28181Node(config));
    std::vector<std::unique_ptr<Capability> > capabilities = createStandardCapabilities();
    for (std::vector<std::unique_ptr<Capability> >::iterator it = capabilities.begin();
         it != capabilities.end();
         ++it)
    {
        node->addCapability(std::move(*it));
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "starting with config " + configPath);
    if (!node->start())
    {
        if (error)
        {
            *error = "failed to start GB28181 node";
        }
        return false;
    }

    std::unique_ptr<ManagementServer> managementServer;
    if (enableManagementServer)
    {
        managementServer.reset(new ManagementServer());
        std::string managementError;
        if (!managementServer->start(config.managementSocketPath(),
                                     [this](const std::string& request) {
                                         return executeManagementCommand(request);
                                     },
                                     &managementError))
        {
            node->stop();
            if (error)
            {
                *error = "management startup failed: " + managementError;
            }
            return false;
        }
    }

    m_node = std::move(node);
    m_managementServer = std::move(managementServer);
    m_running = true;
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "started");
    return true;
}

void MediaFabricRuntime::stop()
{
    std::unique_ptr<GB28181Node> node;
    std::unique_ptr<ManagementServer> managementServer;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running)
        {
            return;
        }
        // Do not join the management thread while holding m_mutex: its command
        // handler also takes this mutex.
        m_running = false;
        managementServer = std::move(m_managementServer);
        node = std::move(m_node);
    }
    if (managementServer)
    {
        managementServer->stop();
    }
    if (node)
    {
        node->stop();
    }
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "stopped");
}

bool MediaFabricRuntime::running() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_running;
}

std::string MediaFabricRuntime::executeManagementCommand(const std::string& request)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || !m_node)
    {
        return "ERROR media-fabric runtime is not running\n";
    }
    return executeCommand(*m_node, request);
}

} // namespace gb28181
