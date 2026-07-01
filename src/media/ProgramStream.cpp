#include "ProgramStream.h"

namespace gb28181 {

namespace {

bool isStartCode(const std::vector<unsigned char>& bytes, size_t offset)
{
    return offset + 3 < bytes.size() &&
           bytes[offset] == 0x00 &&
           bytes[offset + 1] == 0x00 &&
           bytes[offset + 2] == 0x01;
}

uint16_t readUint16(const std::vector<unsigned char>& bytes, size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) |
                                 static_cast<uint16_t>(bytes[offset + 1]));
}

void appendUint16(std::vector<unsigned char>* bytes, uint16_t value)
{
    bytes->push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    bytes->push_back(static_cast<unsigned char>(value & 0xff));
}

bool isPesStreamId(unsigned char streamId)
{
    return streamId == 0xbd ||
           streamId == 0xbe ||
           streamId == 0xbf ||
           streamId == 0xfd ||
           (streamId >= 0xc0 && streamId <= 0xdf) ||
           (streamId >= 0xe0 && streamId <= 0xef);
}

void classifyPes(unsigned char streamId, ProgramStreamInfo* info)
{
    ++info->pesPackets;
    if (streamId >= 0xe0 && streamId <= 0xef)
    {
        ++info->videoPesPackets;
    }
    else if (streamId >= 0xc0 && streamId <= 0xdf)
    {
        ++info->audioPesPackets;
    }
    else if (streamId == 0xbd || streamId == 0xbf || streamId == 0xfd)
    {
        ++info->privatePesPackets;
    }
}

bool isVideoPes(unsigned char streamId)
{
    return streamId >= 0xe0 && streamId <= 0xef;
}

} // namespace

ProgramStreamInfo::ProgramStreamInfo()
    : packHeaders(0),
      systemHeaders(0),
      programStreamMaps(0),
      pesPackets(0),
      videoPesPackets(0),
      audioPesPackets(0),
      privatePesPackets(0),
      pesPayloadBytes(0)
{
}

bool ProgramStreamInfo::hasMediaPayload() const
{
    return videoPesPackets > 0 || audioPesPackets > 0 || privatePesPackets > 0;
}

bool parseProgramStreamPayload(const std::vector<unsigned char>& bytes, ProgramStreamInfo* info)
{
    if (info == NULL || bytes.empty())
    {
        return false;
    }

    ProgramStreamInfo parsed;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        if (!isStartCode(bytes, offset))
        {
            return false;
        }

        const unsigned char streamId = bytes[offset + 3];
        if (streamId == 0xba)
        {
            if (offset + 14 > bytes.size())
            {
                return false;
            }
            const size_t stuffing = bytes[offset + 13] & 0x07;
            offset += 14 + stuffing;
            if (offset > bytes.size())
            {
                return false;
            }
            ++parsed.packHeaders;
            continue;
        }

        if (streamId == 0xbb || streamId == 0xbc)
        {
            if (offset + 6 > bytes.size())
            {
                return false;
            }
            const size_t sectionLength = readUint16(bytes, offset + 4);
            offset += 6 + sectionLength;
            if (offset > bytes.size())
            {
                return false;
            }
            if (streamId == 0xbb)
            {
                ++parsed.systemHeaders;
            }
            else
            {
                ++parsed.programStreamMaps;
            }
            continue;
        }

        if (!isPesStreamId(streamId) || offset + 6 > bytes.size())
        {
            return false;
        }

        classifyPes(streamId, &parsed);
        const size_t packetLength = readUint16(bytes, offset + 4);
        size_t packetEnd = 0;
        if (packetLength == 0)
        {
            packetEnd = bytes.size();
        }
        else
        {
            packetEnd = offset + 6 + packetLength;
            if (packetEnd > bytes.size())
            {
                return false;
            }
        }

        size_t payloadOffset = offset + 6;
        if (streamId != 0xbe && packetEnd >= payloadOffset + 3 && (bytes[payloadOffset] & 0xc0) == 0x80)
        {
            const size_t headerDataLength = bytes[payloadOffset + 2];
            payloadOffset += 3 + headerDataLength;
            if (payloadOffset > packetEnd)
            {
                return false;
            }
        }
        if (streamId != 0xbe)
        {
            parsed.pesPayloadBytes += packetEnd - payloadOffset;
            if (isVideoPes(streamId))
            {
                parsed.videoPayloads.push_back(std::vector<unsigned char>(bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(payloadOffset),
                                                                           bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(packetEnd)));
            }
        }
        offset = packetEnd;
    }

    *info = parsed;
    return parsed.packHeaders > 0 && parsed.hasMediaPayload();
}

std::vector<unsigned char> buildProgramStreamSample(const std::vector<unsigned char>& videoPayload)
{
    std::vector<unsigned char> bytes;

    const unsigned char packHeader[] = {
        0x00, 0x00, 0x01, 0xba,
        0x44, 0x00, 0x04, 0x00,
        0x04, 0x01, 0x00, 0x00,
        0x03, 0xf8
    };
    bytes.insert(bytes.end(), packHeader, packHeader + sizeof(packHeader));

    bytes.push_back(0x00);
    bytes.push_back(0x00);
    bytes.push_back(0x01);
    bytes.push_back(0xe0);
    const size_t pesLength = 3 + videoPayload.size();
    appendUint16(&bytes, pesLength <= 0xffff ? static_cast<uint16_t>(pesLength) : 0);
    bytes.push_back(0x80);
    bytes.push_back(0x00);
    bytes.push_back(0x00);
    bytes.insert(bytes.end(), videoPayload.begin(), videoPayload.end());
    return bytes;
}

} // namespace gb28181
