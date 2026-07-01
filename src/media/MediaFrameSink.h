#ifndef GB28181_MEDIA_MEDIAFRAMESINK_H
#define GB28181_MEDIA_MEDIAFRAMESINK_H

#include "ElementaryStream.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gb28181 {

struct MediaFrame
{
    std::string sessionId;
    std::string peerId;
    VideoCodec codec;
    uint32_t timestamp;
    uint64_t packets;
    bool keyFrame;
    std::vector<unsigned char> data;

    MediaFrame();
};

class MediaFrameSink
{
public:
    virtual ~MediaFrameSink();
    virtual bool writeFrame(const MediaFrame& frame, std::string* error) = 0;
};

class FileMediaFrameSink : public MediaFrameSink
{
public:
    explicit FileMediaFrameSink(const std::string& path);
    bool writeFrame(const MediaFrame& frame, std::string* error);
    size_t frameCount() const;
    uint64_t bytesWritten() const;

private:
    std::string m_path;
    size_t m_frameCount;
    uint64_t m_bytesWritten;
};

} // namespace gb28181

#endif
