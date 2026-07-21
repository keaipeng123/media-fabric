#ifndef GB28181_CORE_NODECONFIG_H
#define GB28181_CORE_NODECONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gb28181 {

struct SipEndpointConfig
{
    std::string name;
    std::string sipId;
    std::string sipIp;
    int sipPort;
    std::string realm;
    std::string username;
    std::string password;
    int rtpPortBegin;
    int rtpPortEnd;

    SipEndpointConfig();
    bool valid() const;
};

struct PeerConfig
{
    std::string name;
    std::string relation;
    std::string sipId;
    std::string remoteIp;
    int remotePort;
    bool registerTo;
    bool allowRegister;
    int expires;
    std::string realm;
    std::string username;
    std::string password;

    PeerConfig();
    bool valid() const;
};

struct MediaConfig
{
    std::string streamFile;
    size_t rtpPayloadBytes;
    uint32_t rtpTimestampIncrement;
    int streamSendIntervalMs;
    bool streamLoop;

    MediaConfig();
};

struct TimerConfig
{
    int heartbeatIntervalSeconds;
    int registerRefreshSeconds;
    TimerConfig();
};

class NodeConfig
{
public:
    NodeConfig();

    bool load(const std::string& configPath);
    const std::string& configPath() const;
    const SipEndpointConfig& node() const;
    const std::vector<SipEndpointConfig>& sipEndpoints() const;
    const std::vector<PeerConfig>& peers() const;
    const MediaConfig& media() const;
    const TimerConfig& timers() const;
    const std::string& managementSocketPath() const;

private:
    std::string m_configPath;
    SipEndpointConfig m_node;
    std::vector<SipEndpointConfig> m_sipEndpoints;
    std::vector<PeerConfig> m_peers;
    MediaConfig m_media;
    TimerConfig m_timers;
    std::string m_managementSocketPath;
};

} // namespace gb28181

#endif
