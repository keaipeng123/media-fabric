#include "ElementaryStream.h"

namespace gb28181 {

namespace {

size_t startCodeSize(const std::vector<unsigned char>& bytes, size_t offset)
{
    if (offset + 3 <= bytes.size() &&
        bytes[offset] == 0x00 &&
        bytes[offset + 1] == 0x00 &&
        bytes[offset + 2] == 0x01)
    {
        return 3;
    }
    if (offset + 4 <= bytes.size() &&
        bytes[offset] == 0x00 &&
        bytes[offset + 1] == 0x00 &&
        bytes[offset + 2] == 0x00 &&
        bytes[offset + 3] == 0x01)
    {
        return 4;
    }
    return 0;
}

size_t findStartCode(const std::vector<unsigned char>& bytes, size_t offset)
{
    for (size_t i = offset; i + 3 <= bytes.size(); ++i)
    {
        if (startCodeSize(bytes, i) > 0)
        {
            return i;
        }
    }
    return bytes.size();
}

void classifyH264(unsigned char header, ElementaryStreamInfo* info)
{
    const unsigned char type = header & 0x1f;
    if (type == 7)
    {
        ++info->h264Sps;
    }
    else if (type == 8)
    {
        ++info->h264Pps;
    }
    else if (type == 5)
    {
        ++info->h264Idr;
    }
}

void classifyH265(unsigned char header, ElementaryStreamInfo* info)
{
    const unsigned char type = static_cast<unsigned char>((header >> 1) & 0x3f);
    if (type == 32)
    {
        ++info->h265Vps;
    }
    else if (type == 33)
    {
        ++info->h265Sps;
    }
    else if (type == 34)
    {
        ++info->h265Pps;
    }
    else if (type >= 16 && type <= 21)
    {
        ++info->h265Idr;
    }
}

VideoCodec detectCodec(unsigned char header)
{
    const unsigned char h264Type = header & 0x1f;
    const unsigned char h265Type = static_cast<unsigned char>((header >> 1) & 0x3f);
    if (h265Type == 32 || h265Type == 33 || h265Type == 34 || (h265Type >= 16 && h265Type <= 21))
    {
        if (!(h264Type == 1 || h264Type == 5 || h264Type == 6 || h264Type == 7 || h264Type == 8))
        {
            return VIDEO_CODEC_H265;
        }
    }
    if (h264Type > 0 && h264Type <= 12)
    {
        return VIDEO_CODEC_H264;
    }
    return VIDEO_CODEC_UNKNOWN;
}

} // namespace

ElementaryStreamInfo::ElementaryStreamInfo()
    : codec(VIDEO_CODEC_UNKNOWN),
      bytes(0),
      nalUnits(0),
      h264Sps(0),
      h264Pps(0),
      h264Idr(0),
      h265Vps(0),
      h265Sps(0),
      h265Pps(0),
      h265Idr(0)
{
}

bool ElementaryStreamInfo::hasVideo() const
{
    return nalUnits > 0 && codec != VIDEO_CODEC_UNKNOWN;
}

bool parseElementaryStream(const std::vector<unsigned char>& bytes, ElementaryStreamInfo* info)
{
    if (info == NULL || bytes.empty())
    {
        return false;
    }

    ElementaryStreamInfo parsed;
    parsed.bytes = bytes.size();

    size_t start = findStartCode(bytes, 0);
    while (start < bytes.size())
    {
        const size_t codeSize = startCodeSize(bytes, start);
        if (codeSize == 0)
        {
            return false;
        }

        const size_t nalBegin = start + codeSize;
        const size_t next = findStartCode(bytes, nalBegin);
        if (nalBegin >= next)
        {
            return false;
        }

        const unsigned char header = bytes[nalBegin];
        const VideoCodec codec = detectCodec(header);
        if (codec == VIDEO_CODEC_UNKNOWN)
        {
            return false;
        }
        if (parsed.codec == VIDEO_CODEC_UNKNOWN)
        {
            parsed.codec = codec;
        }
        else if (parsed.codec != codec)
        {
            return false;
        }

        ++parsed.nalUnits;
        if (codec == VIDEO_CODEC_H264)
        {
            classifyH264(header, &parsed);
        }
        else if (codec == VIDEO_CODEC_H265)
        {
            classifyH265(header, &parsed);
        }

        start = next;
    }

    *info = parsed;
    return parsed.hasVideo();
}

} // namespace gb28181
