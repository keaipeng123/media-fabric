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

MediaConfig readMediaConfig(const IniData& data)
{
    MediaConfig media;
    media.streamFile = getValue(data, "media", "stream_file");
    media.rtpPayloadBytes = toSize(getValue(data, "media", "rtp_payload_bytes"), media.rtpPayloadBytes);
    media.rtpTimestampIncrement = toUint32(getValue(data, "media", "rtp_timestamp_increment"),
                                           media.rtpTimestampIncrement);
    return media;
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

MediaConfig::MediaConfig()
    : streamFile(),
      rtpPayloadBytes(1300),
      rtpTimestampIncrement(3600)
{
}

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

    std::vector<SipEndpointConfig> endpoints;
    const char* knownSections[] = {"node", "sup", "sub"};
    for (size_t i = 0; i < sizeof(knownSections) / sizeof(knownSections[0]); ++i)
    {
        SipEndpointConfig endpoint = readEndpoint(data, knownSections[i]);
        if (endpoint.valid())
        {
            endpoints.push_back(endpoint);
        }
    }

    if (endpoints.empty())
    {
        return false;
    }

    m_configPath = configPath;
    m_sipEndpoints = endpoints;
    m_media = readMediaConfig(data);
    return true;
}

const std::string& NodeConfig::configPath() const
{
    return m_configPath;
}

const std::vector<SipEndpointConfig>& NodeConfig::sipEndpoints() const
{
    return m_sipEndpoints;
}

const MediaConfig& NodeConfig::media() const
{
    return m_media;
}

} // namespace gb28181
