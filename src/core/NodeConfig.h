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

struct MediaConfig
{
    std::string streamFile;
    size_t rtpPayloadBytes;
    uint32_t rtpTimestampIncrement;

    MediaConfig();
};

class NodeConfig
{
public:
    NodeConfig();

    bool load(const std::string& configPath);
    const std::string& configPath() const;
    const std::vector<SipEndpointConfig>& sipEndpoints() const;
    const MediaConfig& media() const;

private:
    std::string m_configPath;
    std::vector<SipEndpointConfig> m_sipEndpoints;
    MediaConfig m_media;
};

} // namespace gb28181

#endif
