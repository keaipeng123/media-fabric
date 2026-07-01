#ifndef GB28181_MEDIA_SDPSESSION_H
#define GB28181_MEDIA_SDPSESSION_H

#include <string>

namespace gb28181 {

struct SdpInfo
{
    std::string originUser;
    std::string sessionName;
    std::string connectionIp;
    int mediaPort;
    std::string transport;
    std::string direction;
    std::string setup;
    std::string ssrc;
    long startTime;
    long stopTime;

    SdpInfo();
    bool valid() const;
};

bool parseSdp(const std::string& body, SdpInfo* info);
std::string buildInviteSdp(const std::string& deviceId,
                           const std::string& localIp,
                           int localRtpPort,
                           const std::string& ssrc);
std::string buildInviteResponseSdp(const std::string& deviceId,
                                   const std::string& localIp,
                                   int localRtpPort,
                                   const SdpInfo& request);

} // namespace gb28181

#endif
