#ifndef GB28181_MEDIA_RTPSESSIONADAPTER_H
#define GB28181_MEDIA_RTPSESSIONADAPTER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gb28181 {

struct RtpPayloadPacket
{
    std::vector<unsigned char> payload;
    uint8_t payloadType;
    bool marker;
    uint32_t timestampIncrement;

    RtpPayloadPacket();
};

struct RtpSessionOptions
{
    std::string sessionId;
    int localRtpPort;
    std::string remoteIp;
    int remoteRtpPort;
    size_t maximumPacketSize;
    int receiveBufferBytes;
    int sendBufferBytes;
    int multicastTtl;
    bool usePollThread;

    RtpSessionOptions();
};

typedef std::function<void(const std::string& sessionId, const std::vector<unsigned char>& packetBytes)> RtpPacketCallback;

class RtpSessionAdapter
{
public:
    virtual ~RtpSessionAdapter();
    virtual bool start(const RtpSessionOptions& options, RtpPacketCallback callback, std::string* error) = 0;
    virtual bool sendPayloadPacket(const RtpPayloadPacket& packet, std::string* error) = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;
};

} // namespace gb28181

#endif
