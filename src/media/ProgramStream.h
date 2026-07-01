#ifndef GB28181_MEDIA_PROGRAMSTREAM_H
#define GB28181_MEDIA_PROGRAMSTREAM_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gb28181 {

struct ProgramStreamInfo
{
    size_t packHeaders;
    size_t systemHeaders;
    size_t programStreamMaps;
    size_t pesPackets;
    size_t videoPesPackets;
    size_t audioPesPackets;
    size_t privatePesPackets;
    size_t pesPayloadBytes;
    std::vector<std::vector<unsigned char> > videoPayloads;

    ProgramStreamInfo();
    bool hasMediaPayload() const;
};

bool parseProgramStreamPayload(const std::vector<unsigned char>& bytes, ProgramStreamInfo* info);
std::vector<unsigned char> buildProgramStreamSample(const std::vector<unsigned char>& videoPayload);

} // namespace gb28181

#endif
