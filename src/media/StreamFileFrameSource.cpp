#include "StreamFileFrameSource.h"

#include "StreamHeader.h"

#include <limits>

namespace gb28181 {
namespace {

const int kVideoFrameType = 2;
const int kMaxFrameBytes = 16 * 1024 * 1024;

void setError(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

} // namespace

StreamFileFrame::StreamFileFrame()
    : type(0),
      pts(0),
      keyFrame(false),
      data()
{
}

StreamFileFrameSource::StreamFileFrameSource()
{
}

StreamFileFrameSource::StreamFileFrameSource(const std::string& path)
{
    std::string ignoredError;
    open(path, &ignoredError);
}

bool StreamFileFrameSource::open(const std::string& path, std::string* error)
{
    if (path.empty())
    {
        setError(error, "empty stream file path");
        return false;
    }

    m_input.close();
    m_input.clear();
    m_input.open(path.c_str(), std::ios::in | std::ios::binary);
    if (!m_input)
    {
        setError(error, "failed to open stream file: " + path);
        return false;
    }

    m_path = path;
    return true;
}

bool StreamFileFrameSource::readNextVideoFrame(StreamFileFrame* frame, std::string* error)
{
    if (frame == NULL)
    {
        setError(error, "empty stream frame output");
        return false;
    }
    if (!m_input)
    {
        setError(error, "stream file is not open");
        return false;
    }

    while (m_input)
    {
        StreamHeader header;
        m_input.read(reinterpret_cast<char*>(&header), static_cast<std::streamsize>(sizeof(header)));
        if (m_input.gcount() == 0 && m_input.eof())
        {
            setError(error, "end of stream file");
            return false;
        }
        if (!m_input || m_input.gcount() != static_cast<std::streamsize>(sizeof(header)))
        {
            setError(error, "failed to read stream frame header: " + m_path);
            return false;
        }
        if (header.length <= 0 || header.length > kMaxFrameBytes)
        {
            setError(error, "invalid stream frame length");
            return false;
        }

        std::vector<unsigned char> data(static_cast<size_t>(header.length));
        m_input.read(reinterpret_cast<char*>(&data[0]), static_cast<std::streamsize>(data.size()));
        if (!m_input || m_input.gcount() != static_cast<std::streamsize>(data.size()))
        {
            setError(error, "failed to read stream frame payload: " + m_path);
            return false;
        }
        if (header.type != kVideoFrameType)
        {
            continue;
        }

        StreamFileFrame parsed;
        parsed.type = header.type;
        parsed.pts = header.pts;
        parsed.keyFrame = header.keyFrame != 0;
        parsed.data = data;
        *frame = parsed;
        return true;
    }

    setError(error, "end of stream file");
    return false;
}

const std::string& StreamFileFrameSource::path() const
{
    return m_path;
}

} // namespace gb28181
