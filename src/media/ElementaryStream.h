#ifndef GB28181_MEDIA_ELEMENTARYSTREAM_H
#define GB28181_MEDIA_ELEMENTARYSTREAM_H

#include <cstddef>
#include <vector>

namespace gb28181 {

enum VideoCodec
{
    VIDEO_CODEC_UNKNOWN,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265
};

struct ElementaryStreamInfo
{
    VideoCodec codec;
    size_t bytes;
    size_t nalUnits;
    size_t h264Sps;
    size_t h264Pps;
    size_t h264Idr;
    size_t h265Vps;
    size_t h265Sps;
    size_t h265Pps;
    size_t h265Idr;

    ElementaryStreamInfo();
    bool hasVideo() const;
};

bool parseElementaryStream(const std::vector<unsigned char>& bytes, ElementaryStreamInfo* info);

} // namespace gb28181

#endif
