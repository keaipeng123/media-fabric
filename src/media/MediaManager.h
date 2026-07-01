#ifndef GB28181_MEDIA_MEDIAMANAGER_H
#define GB28181_MEDIA_MEDIAMANAGER_H

#include "ElementaryStream.h"
#include "MediaFrameSink.h"
#include "NodeConfig.h"
#include "ProgramStream.h"
#include "RtpPacket.h"
#include "RtpSessionAdapter.h"

#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace gb28181 {

struct MediaSessionInfo
{
    std::string id;
    std::string peerId;
    std::string localIp;
    int localRtpPort;
    std::string remoteIp;
    int remoteRtpPort;
    std::string transport;
    std::string direction;
    std::string ssrc;
    std::string callId;
    std::string inviteCseq;
    std::string remoteContact;
    std::string state;
    uint64_t receivedRtpPackets;
    uint64_t receivedRtpBytes;
    uint64_t receivedPayloadBytes;
    uint64_t sentRtpPayloadPackets;
    uint64_t sentRtpPayloadBytes;
    uint64_t sentVideoFrames;
    uint64_t receivedPsPacks;
    uint64_t receivedPesPackets;
    uint64_t receivedVideoPesPackets;
    uint64_t receivedAudioPesPackets;
    uint64_t receivedPsPayloadBytes;
    uint64_t receivedVideoNalUnits;
    uint64_t receivedH264NalUnits;
    uint64_t receivedH265NalUnits;
    uint64_t receivedH264Sps;
    uint64_t receivedH264Pps;
    uint64_t receivedH264Idr;
    uint64_t receivedH265Vps;
    uint64_t receivedH265Sps;
    uint64_t receivedH265Pps;
    uint64_t receivedH265Idr;
    uint64_t completedVideoFrames;
    uint64_t incompleteVideoFrames;
    uint64_t incompleteRtpPayloads;
    uint64_t currentRtpPayloadBytes;
    uint64_t currentRtpPayloadPackets;
    uint32_t currentRtpPayloadTimestamp;
    bool hasCurrentRtpPayload;
    std::vector<unsigned char> currentRtpPayloadData;
    uint64_t currentVideoFrameBytes;
    uint64_t currentVideoFramePackets;
    uint64_t lastVideoFrameBytes;
    uint64_t lastVideoFramePackets;
    uint32_t currentVideoFrameTimestamp;
    VideoCodec currentVideoFrameCodec;
    VideoCodec lastVideoFrameCodec;
    bool currentVideoFrameKey;
    bool lastVideoFrameKey;
    bool hasCurrentVideoFrame;
    std::vector<unsigned char> currentVideoFrameData;
    uint32_t lastRtpTimestamp;
    uint32_t lastRtpSsrc;
    uint16_t lastRtpSequence;
    uint64_t outOfOrderRtpPackets;
    bool hasRtpSequence;

    MediaSessionInfo();
};

class MediaManager
{
public:
    bool configure(const NodeConfig& config);
    void setFrameSink(std::unique_ptr<MediaFrameSink> sink);
    int allocateRtpPort();
    void releaseRtpPort(int port);
    bool createSession(const MediaSessionInfo& session);
    const MediaSessionInfo* findSession(const std::string& sessionId) const;
    bool updateSessionState(const std::string& sessionId, const std::string& state);
    std::vector<MediaSessionInfo> sessionSnapshot() const;
    bool startRtpSessionAdapter(const std::string& sessionId,
                                std::unique_ptr<RtpSessionAdapter> adapter,
                                std::string* error);
    void stopRtpSessionAdapter(const std::string& sessionId);
    bool rtpSessionAdapterRunning(const std::string& sessionId) const;
    bool receiveRtpPacket(const std::string& sessionId,
                          const std::vector<unsigned char>& packetBytes,
                          std::string* error);
    bool buildOutboundRtpPackets(const std::string& sessionId,
                                 const std::vector<unsigned char>& annexBFrame,
                                 uint16_t firstSequenceNumber,
                                 uint32_t timestamp,
                                 size_t maxPayloadBytes,
                                 std::vector<std::vector<unsigned char> >* packets,
                                 std::string* error) const;
    bool sendAnnexBFrame(const std::string& sessionId,
                         const std::vector<unsigned char>& annexBFrame,
                         uint32_t timestampIncrement,
                         size_t maxPayloadBytes,
                         std::string* error);
    bool closeSession(const std::string& sessionId);
    size_t availablePortCount() const;
    size_t sessionCount() const;

private:
    std::queue<int> m_availablePorts;
    std::set<int> m_availablePortSet;
    std::set<int> m_allocatedPorts;
    std::map<std::string, MediaSessionInfo> m_sessions;
    std::map<std::string, std::unique_ptr<RtpSessionAdapter> > m_rtpSessionAdapters;
    std::unique_ptr<MediaFrameSink> m_frameSink;
};

} // namespace gb28181

#endif
