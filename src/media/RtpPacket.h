#ifndef GB28181_MEDIA_RTPPACKET_H
#define GB28181_MEDIA_RTPPACKET_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gb28181 {

struct RtpPacket
{
    bool marker;
    uint8_t payloadType;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
    std::vector<unsigned char> payload;

    RtpPacket();
};

struct RtpPacketizationOptions
{
    uint8_t payloadType;
    uint16_t firstSequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
    size_t maxPayloadBytes;

    RtpPacketizationOptions();
};

bool parseRtpPacket(const std::vector<unsigned char>& bytes, RtpPacket* packet);
std::vector<unsigned char> buildRtpPacket(uint8_t payloadType,
                                          bool marker,
                                          uint16_t sequenceNumber,
                                          uint32_t timestamp,
                                          uint32_t ssrc,
                                          const std::vector<unsigned char>& payload);
bool packetizeRtpPayload(const std::vector<unsigned char>& payload,
                         const RtpPacketizationOptions& options,
                         std::vector<std::vector<unsigned char> >* packets);

} // namespace gb28181

#endif
