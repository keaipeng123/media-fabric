#include "NodeConfig.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace gb28181 {
namespace {

std::string trim(const std::string& value)
{
    std::string::size_type begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    std::string::size_type end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

int toInt(const std::string& value)
{
    std::istringstream input(value);
    int result = 0;
    input >> result;
    return result;
}

size_t toSize(const std::string& value, size_t fallback)
{
    if (value.empty())
    {
        return fallback;
    }

    std::istringstream input(value);
    size_t result = fallback;
    input >> result;
    return input ? result : fallback;
}

uint32_t toUint32(const std::string& value, uint32_t fallback)
{
    if (value.empty())
    {
        return fallback;
    }

    std::istringstream input(value);
    uint32_t result = fallback;
    input >> result;
    return input ? result : fallback;
}

bool toBool(const std::string& value, bool fallback)
{
    if (value.empty())
    {
        return fallback;
    }

    std::string normalized;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*it))));
    }

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
    {
        return false;
    }
    return fallback;
}

typedef std::map<std::string, std::map<std::string, std::string> > IniData;

std::string getValue(const IniData& data, const std::string& section, const std::string& key)
{
    IniData::const_iterator secIt = data.find(section);
    if (secIt == data.end())
    {
        return "";
    }

    std::map<std::string, std::string>::const_iterator valueIt = secIt->second.find(key);
    if (valueIt == secIt->second.end())
    {
        return "";
    }

    return valueIt->second;
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0;
}

SipEndpointConfig readEndpoint(const IniData& data, const std::string& section)
{
    SipEndpointConfig endpoint;
    endpoint.name = section;
    endpoint.sipId = getValue(data, section, "sip_id");
    endpoint.sipIp = getValue(data, section, "sip_ip");
    endpoint.sipPort = toInt(getValue(data, section, "sip_port"));
    endpoint.realm = getValue(data, section, "sip_realm");
    endpoint.username = getValue(data, section, "sip_usr");
    endpoint.password = getValue(data, section, "sip_pwd");
    endpoint.rtpPortBegin = toInt(getValue(data, section, "rtp_port_begin"));
    endpoint.rtpPortEnd = toInt(getValue(data, section, "rtp_port_end"));
    return endpoint;
}

PeerConfig readPeer(const IniData& data, const std::string& section, const std::string& relation)
{
    PeerConfig peer;
    peer.name = section;
    peer.relation = relation;
    peer.sipId = getValue(data, section, "sip_id");
    peer.remoteIp = getValue(data, section, "remote_ip");
    peer.remotePort = toInt(getValue(data, section, "remote_port"));
    peer.registerTo = toBool(getValue(data, section, "register_to"), relation == "upstream");
    peer.allowRegister = toBool(getValue(data, section, "allow_register"), relation == "downstream");
    peer.expires = toInt(getValue(data, section, "expires"));
    peer.realm = getValue(data, section, "sip_realm");
    peer.username = getValue(data, section, "sip_usr");
    peer.password = getValue(data, section, "sip_pwd");
    return peer;
}

MediaConfig readMediaConfig(const IniData& data)
{
    MediaConfig media;
    media.streamFile = getValue(data, "media", "stream_file");
    media.catalogFile = getValue(data, "media", "catalog_file");
    media.rtpPayloadBytes = toSize(getValue(data, "media", "rtp_payload_bytes"), media.rtpPayloadBytes);
    media.rtpTimestampIncrement = toUint32(getValue(data, "media", "rtp_timestamp_increment"),
                                           media.rtpTimestampIncrement);
    media.streamSendIntervalMs = toInt(getValue(data, "media", "stream_send_interval_ms"));
    if (media.streamSendIntervalMs <= 0)
    {
        media.streamSendIntervalMs = MediaConfig().streamSendIntervalMs;
    }
    media.streamLoop = toBool(getValue(data, "media", "stream_loop"), media.streamLoop);
    return media;
}

TimerConfig readTimerConfig(const IniData& data)
{
    TimerConfig timers;
    timers.heartbeatIntervalSeconds = toInt(getValue(data, "timers", "heartbeat_interval_seconds"));
    timers.registerRefreshSeconds = toInt(getValue(data, "timers", "register_refresh_seconds"));
    if (timers.heartbeatIntervalSeconds <= 0) timers.heartbeatIntervalSeconds = TimerConfig().heartbeatIntervalSeconds;
    if (timers.registerRefreshSeconds <= 0) timers.registerRefreshSeconds = TimerConfig().registerRefreshSeconds;
    return timers;
}

} // namespace

SipEndpointConfig::SipEndpointConfig()
    : sipPort(0),
      rtpPortBegin(0),
      rtpPortEnd(0)
{
}

bool SipEndpointConfig::valid() const
{
    return !sipId.empty() && !sipIp.empty() && sipPort > 0;
}

PeerConfig::PeerConfig()
    : remotePort(0),
      registerTo(false),
      allowRegister(false),
      expires(0)
{
}

bool PeerConfig::valid() const
{
    return !sipId.empty() && (relation == "upstream" || relation == "downstream");
}

MediaConfig::MediaConfig()
    : streamFile(),
      rtpPayloadBytes(1300),
      rtpTimestampIncrement(3600),
      streamSendIntervalMs(1000),
      streamLoop(true)
{
}

TimerConfig::TimerConfig() : heartbeatIntervalSeconds(60), registerRefreshSeconds(300) {}

NodeConfig::NodeConfig()
{
}

bool NodeConfig::load(const std::string& configPath)
{
    if (configPath.empty())
    {
        return false;
    }

    std::ifstream input(configPath.c_str());
    if (!input.good())
    {
        return false;
    }

    IniData data;
    std::string currentSection;
    std::string line;
    while (std::getline(input, line))
    {
        std::string content = trim(line);
        if (content.empty() || content[0] == '#' || content[0] == ';')
        {
            continue;
        }

        if (content[0] == '[' && content[content.size() - 1] == ']')
        {
            currentSection = trim(content.substr(1, content.size() - 2));
            continue;
        }

        std::string::size_type eq = content.find('=');
        if (eq == std::string::npos || currentSection.empty())
        {
            continue;
        }

        const std::string key = trim(content.substr(0, eq));
        const std::string value = trim(content.substr(eq + 1));
        data[currentSection][key] = value;
    }

    SipEndpointConfig node = readEndpoint(data, "node");
    if (!node.valid())
    {
        SipEndpointConfig legacySub = readEndpoint(data, "sub");
        SipEndpointConfig legacySup = readEndpoint(data, "sup");
        if (legacySub.valid())
        {
            node = legacySub;
            node.name = "node";
        }
        else if (legacySup.valid())
        {
            node = legacySup;
            node.name = "node";
        }
    }

    if (!node.valid())
    {
        return false;
    }

    std::vector<SipEndpointConfig> endpoints;
    endpoints.push_back(node);

    std::vector<PeerConfig> peers;
    for (IniData::const_iterator it = data.begin(); it != data.end(); ++it)
    {
        if (startsWith(it->first, "peer.upstream."))
        {
            PeerConfig peer = readPeer(data, it->first, "upstream");
            if (peer.valid())
            {
                peers.push_back(peer);
            }
        }
        else if (startsWith(it->first, "peer.downstream."))
        {
            PeerConfig peer = readPeer(data, it->first, "downstream");
            if (peer.valid())
            {
                peers.push_back(peer);
            }
        }
    }

    if (peers.empty())
    {
        SipEndpointConfig legacySup = readEndpoint(data, "sup");
        if (legacySup.valid())
        {
            PeerConfig peer;
            peer.name = "peer.upstream.legacy";
            peer.relation = "upstream";
            peer.sipId = legacySup.sipId;
            peer.remoteIp = legacySup.sipIp;
            peer.remotePort = legacySup.sipPort;
            peer.registerTo = true;
            peer.expires = 300;
            peer.realm = legacySup.realm;
            peer.username = legacySup.username;
            peer.password = legacySup.password;
            peers.push_back(peer);
        }

        SipEndpointConfig legacySub = readEndpoint(data, "sub");
        if (legacySub.valid())
        {
            PeerConfig peer;
            peer.name = "peer.downstream.legacy";
            peer.relation = "downstream";
            peer.sipId = legacySub.sipId;
            peer.remoteIp = legacySub.sipIp;
            peer.remotePort = legacySub.sipPort;
            peer.allowRegister = true;
            peers.push_back(peer);
        }
    }

    m_configPath = configPath;
    m_node = node;
    m_sipEndpoints = endpoints;
    m_peers = peers;
    m_media = readMediaConfig(data);
    m_timers = readTimerConfig(data);
    m_managementSocketPath = getValue(data, "management", "socket_path");
    if (m_managementSocketPath.empty()) m_managementSocketPath = "/tmp/media-fabric.sock";
    return true;
}

const std::string& NodeConfig::configPath() const
{
    return m_configPath;
}

const SipEndpointConfig& NodeConfig::node() const
{
    return m_node;
}

const std::vector<SipEndpointConfig>& NodeConfig::sipEndpoints() const
{
    return m_sipEndpoints;
}

const std::vector<PeerConfig>& NodeConfig::peers() const
{
    return m_peers;
}

const MediaConfig& NodeConfig::media() const
{
    return m_media;
}

const TimerConfig& NodeConfig::timers() const { return m_timers; }
const std::string& NodeConfig::managementSocketPath() const { return m_managementSocketPath; }

} // namespace gb28181
