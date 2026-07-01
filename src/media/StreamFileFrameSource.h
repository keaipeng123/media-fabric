#ifndef GB28181_MEDIA_STREAMFILEFRAMESOURCE_H
#define GB28181_MEDIA_STREAMFILEFRAMESOURCE_H

#include <fstream>
#include <string>
#include <vector>

namespace gb28181 {

struct StreamFileFrame
{
    int type;
    int pts;
    bool keyFrame;
    std::vector<unsigned char> data;

    StreamFileFrame();
};

class StreamFileFrameSource
{
public:
    StreamFileFrameSource();
    explicit StreamFileFrameSource(const std::string& path);

    bool open(const std::string& path, std::string* error);
    bool readNextVideoFrame(StreamFileFrame* frame, std::string* error);
    const std::string& path() const;

private:
    std::string m_path;
    std::ifstream m_input;
};

} // namespace gb28181

#endif
