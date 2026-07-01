#include "MediaFrameSink.h"

#include <fstream>

namespace gb28181 {

namespace {

void setError(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

} // namespace

MediaFrame::MediaFrame()
    : codec(VIDEO_CODEC_UNKNOWN),
      timestamp(0),
      packets(0),
      keyFrame(false)
{
}

MediaFrameSink::~MediaFrameSink()
{
}

FileMediaFrameSink::FileMediaFrameSink(const std::string& path)
    : m_path(path),
      m_frameCount(0),
      m_bytesWritten(0)
{
    std::ofstream reset(m_path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
}

bool FileMediaFrameSink::writeFrame(const MediaFrame& frame, std::string* error)
{
    if (m_path.empty())
    {
        setError(error, "empty media frame output path");
        return false;
    }
    if (frame.data.empty())
    {
        setError(error, "empty media frame");
        return false;
    }

    std::ofstream output(m_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    if (!output)
    {
        setError(error, "failed to open media frame output: " + m_path);
        return false;
    }
    output.write(reinterpret_cast<const char*>(&frame.data[0]), static_cast<std::streamsize>(frame.data.size()));
    if (!output)
    {
        setError(error, "failed to write media frame output: " + m_path);
        return false;
    }

    ++m_frameCount;
    m_bytesWritten += frame.data.size();
    return true;
}

size_t FileMediaFrameSink::frameCount() const
{
    return m_frameCount;
}

uint64_t FileMediaFrameSink::bytesWritten() const
{
    return m_bytesWritten;
}

} // namespace gb28181
