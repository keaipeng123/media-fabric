#include "RtpPacket.h"

#include <cstddef>

namespace gb28181 {

namespace {

uint16_t readUint16(const std::vector<unsigned char>& bytes, size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) |
                                 static_cast<uint16_t>(bytes[offset + 1]));
}

uint32_t readUint32(const std::vector<unsigned char>& bytes, size_t offset)
{
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

void appendUint16(std::vector<unsigned char>* bytes, uint16_t value)
{
    bytes->push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    bytes->push_back(static_cast<unsigned char>(value & 0xff));
}

void appendUint32(std::vector<unsigned char>* bytes, uint32_t value)
{
    bytes->push_back(static_cast<unsigned char>((value >> 24) & 0xff));
    bytes->push_back(static_cast<unsigned char>((value >> 16) & 0xff));
    bytes->push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    bytes->push_back(static_cast<unsigned char>(value & 0xff));
}

} // namespace

RtpPacket::RtpPacket()
    : marker(false),
      payloadType(0),
      sequenceNumber(0),
      timestamp(0),
      ssrc(0)
{
}

RtpPacketizationOptions::RtpPacketizationOptions()
    : payloadType(96),
      firstSequenceNumber(0),
      timestamp(0),
      ssrc(0),
      maxPayloadBytes(1300)
{
}

bool parseRtpPacket(const std::vector<unsigned char>& bytes, RtpPacket* packet)
{
    if (packet == NULL || bytes.size() < 12)
    {
        return false;
    }

    const unsigned char first = bytes[0];
    const unsigned char version = static_cast<unsigned char>((first >> 6) & 0x03);
    if (version != 2)
    {
        return false;
    }

    const bool hasPadding = (first & 0x20) != 0;
    const bool hasExtension = (first & 0x10) != 0;
    const size_t csrcCount = first & 0x0f;
    size_t offset = 12 + csrcCount * 4;
    if (bytes.size() < offset)
    {
        return false;
    }

    if (hasExtension)
    {
        if (bytes.size() < offset + 4)
        {
            return false;
        }
        const size_t extensionWords = readUint16(bytes, offset + 2);
        offset += 4 + extensionWords * 4;
        if (bytes.size() < offset)
        {
            return false;
        }
    }

    size_t payloadEnd = bytes.size();
    if (hasPadding)
    {
        if (bytes.empty())
        {
            return false;
        }
        const size_t padding = bytes.back();
        if (padding == 0 || padding > payloadEnd - offset)
        {
            return false;
        }
        payloadEnd -= padding;
    }

    RtpPacket parsed;
    parsed.marker = (bytes[1] & 0x80) != 0;
    parsed.payloadType = static_cast<uint8_t>(bytes[1] & 0x7f);
    parsed.sequenceNumber = readUint16(bytes, 2);
    parsed.timestamp = readUint32(bytes, 4);
    parsed.ssrc = readUint32(bytes, 8);
    parsed.payload.assign(bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset),
                          bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(payloadEnd));

    *packet = parsed;
    return true;
}

std::vector<unsigned char> buildRtpPacket(uint8_t payloadType,
                                          bool marker,
                                          uint16_t sequenceNumber,
                                          uint32_t timestamp,
                                          uint32_t ssrc,
                                          const std::vector<unsigned char>& payload)
{
    std::vector<unsigned char> bytes;
    bytes.reserve(12 + payload.size());
    bytes.push_back(0x80);
    bytes.push_back(static_cast<unsigned char>((marker ? 0x80 : 0x00) | (payloadType & 0x7f)));
    appendUint16(&bytes, sequenceNumber);
    appendUint32(&bytes, timestamp);
    appendUint32(&bytes, ssrc);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

bool packetizeRtpPayload(const std::vector<unsigned char>& payload,
                         const RtpPacketizationOptions& options,
                         std::vector<std::vector<unsigned char> >* packets)
{
    if (packets == NULL || payload.empty() || options.maxPayloadBytes == 0)
    {
        return false;
    }

    std::vector<std::vector<unsigned char> > built;
    size_t offset = 0;
    uint16_t sequence = options.firstSequenceNumber;
    while (offset < payload.size())
    {
        const size_t remaining = payload.size() - offset;
        const size_t chunkBytes = remaining > options.maxPayloadBytes ? options.maxPayloadBytes : remaining;
        const bool marker = offset + chunkBytes >= payload.size();
        const std::vector<unsigned char> chunk(payload.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset),
                                               payload.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset + chunkBytes));
        built.push_back(buildRtpPacket(options.payloadType,
                                       marker,
                                       sequence,
                                       options.timestamp,
                                       options.ssrc,
                                       chunk));
        ++sequence;
        offset += chunkBytes;
    }

    *packets = built;
    return !packets->empty();
}

} // namespace gb28181
