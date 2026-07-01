#include "SdpSession.h"

#include <sstream>
#include <vector>

namespace gb28181 {
namespace {

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::string part;
    std::istringstream input(value);
    while (std::getline(input, part, delimiter))
    {
        if (!part.empty())
        {
            parts.push_back(part);
        }
    }
    return parts;
}

std::string trimLine(const std::string& value)
{
    std::string::size_type end = value.size();
    while (end > 0 && (value[end - 1] == '\r' || value[end - 1] == '\n' || value[end - 1] == ' ' || value[end - 1] == '\t'))
    {
        --end;
    }
    std::string::size_type begin = 0;
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
    {
        ++begin;
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

long toLong(const std::string& value)
{
    std::istringstream input(value);
    long result = 0;
    input >> result;
    return result;
}

std::string responseSetup(const std::string& requestSetup)
{
    if (requestSetup == "active")
    {
        return "passive";
    }
    if (requestSetup == "passive")
    {
        return "active";
    }
    return "";
}

} // namespace

SdpInfo::SdpInfo()
    : mediaPort(0),
      startTime(0),
      stopTime(0)
{
}

bool SdpInfo::valid() const
{
    return !connectionIp.empty() && mediaPort > 0 && !transport.empty();
}

bool parseSdp(const std::string& body, SdpInfo* info)
{
    if (body.empty() || info == NULL)
    {
        return false;
    }

    SdpInfo parsed;
    std::istringstream input(body);
    std::string line;
    while (std::getline(input, line))
    {
        line = trimLine(line);
        if (line.size() < 2 || line[1] != '=')
        {
            continue;
        }

        const std::string value = line.substr(2);
        if (line[0] == 'o')
        {
            std::vector<std::string> parts = split(value, ' ');
            if (!parts.empty())
            {
                parsed.originUser = parts[0];
            }
        }
        else if (line[0] == 's')
        {
            parsed.sessionName = value;
        }
        else if (line[0] == 'c')
        {
            std::vector<std::string> parts = split(value, ' ');
            if (parts.size() >= 3)
            {
                parsed.connectionIp = parts[2];
            }
        }
        else if (line[0] == 't')
        {
            std::vector<std::string> parts = split(value, ' ');
            if (parts.size() >= 2)
            {
                parsed.startTime = toLong(parts[0]);
                parsed.stopTime = toLong(parts[1]);
            }
        }
        else if (line[0] == 'm')
        {
            std::vector<std::string> parts = split(value, ' ');
            if (parts.size() >= 3 && parts[0] == "video")
            {
                parsed.mediaPort = toInt(parts[1]);
                parsed.transport = parts[2];
            }
        }
        else if (line[0] == 'a')
        {
            if (value == "recvonly" || value == "sendonly" || value == "sendrecv")
            {
                parsed.direction = value;
            }
            else if (value.find("setup:") == 0)
            {
                parsed.setup = value.substr(6);
            }
        }
        else if (line[0] == 'y')
        {
            parsed.ssrc = value;
        }
    }

    if (!parsed.valid())
    {
        return false;
    }

    *info = parsed;
    return true;
}

std::string buildInviteSdp(const std::string& deviceId,
                           const std::string& localIp,
                           int localRtpPort,
                           const std::string& ssrc)
{
    std::ostringstream body;
    body << "v=0\r\n"
         << "o=" << deviceId << " 0 0 IN IP4 " << localIp << "\r\n"
         << "s=Play\r\n"
         << "c=IN IP4 " << localIp << "\r\n"
         << "t=0 0\r\n"
         << "m=video " << localRtpPort << " RTP/AVP 96\r\n"
         << "a=recvonly\r\n"
         << "a=rtpmap:96 PS/90000\r\n"
         << "y=" << ssrc << "\r\n";
    return body.str();
}

std::string buildInviteResponseSdp(const std::string& deviceId,
                                   const std::string& localIp,
                                   int localRtpPort,
                                   const SdpInfo& request)
{
    const std::string transport = request.transport.empty() ? "RTP/AVP" : request.transport;
    std::ostringstream body;
    body << "v=0\r\n"
         << "o=" << deviceId << " 0 0 IN IP4 " << localIp << "\r\n"
         << "s=" << (request.sessionName.empty() ? "Play" : request.sessionName) << "\r\n"
         << "c=IN IP4 " << localIp << "\r\n"
         << "t=" << request.startTime << " " << request.stopTime << "\r\n"
         << "m=video " << localRtpPort << " " << transport << " 96\r\n"
         << "a=rtpmap:96 PS/90000\r\n"
         << "a=sendonly\r\n";
    const std::string setup = responseSetup(request.setup);
    if (!setup.empty())
    {
        body << "a=setup:" << setup << "\r\n";
    }
    if (!request.ssrc.empty())
    {
        body << "y=" << request.ssrc << "\r\n";
    }
    return body.str();
}

} // namespace gb28181
