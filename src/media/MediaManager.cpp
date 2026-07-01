#include "MediaManager.h"

#include <sstream>

namespace gb28181 {

MediaSessionInfo::MediaSessionInfo()
    : localRtpPort(0),
      remoteRtpPort(0),
      receivedRtpPackets(0),
      receivedRtpBytes(0),
      receivedPayloadBytes(0),
      sentRtpPayloadPackets(0),
      sentRtpPayloadBytes(0),
      sentVideoFrames(0),
      receivedPsPacks(0),
      receivedPesPackets(0),
      receivedVideoPesPackets(0),
      receivedAudioPesPackets(0),
      receivedPsPayloadBytes(0),
      receivedVideoNalUnits(0),
      receivedH264NalUnits(0),
      receivedH265NalUnits(0),
      receivedH264Sps(0),
      receivedH264Pps(0),
      receivedH264Idr(0),
      receivedH265Vps(0),
      receivedH265Sps(0),
      receivedH265Pps(0),
      receivedH265Idr(0),
      completedVideoFrames(0),
      incompleteVideoFrames(0),
      incompleteRtpPayloads(0),
      currentRtpPayloadBytes(0),
      currentRtpPayloadPackets(0),
      currentRtpPayloadTimestamp(0),
      hasCurrentRtpPayload(false),
      currentVideoFrameBytes(0),
      currentVideoFramePackets(0),
      lastVideoFrameBytes(0),
      lastVideoFramePackets(0),
      currentVideoFrameTimestamp(0),
      currentVideoFrameCodec(VIDEO_CODEC_UNKNOWN),
      lastVideoFrameCodec(VIDEO_CODEC_UNKNOWN),
      currentVideoFrameKey(false),
      lastVideoFrameKey(false),
      hasCurrentVideoFrame(false),
      lastRtpTimestamp(0),
      lastRtpSsrc(0),
      lastRtpSequence(0),
      outOfOrderRtpPackets(0),
      hasRtpSequence(false)
{
}

namespace {

void setError(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

std::string uint32ToString(uint32_t value)
{
    std::ostringstream output;
    output << value;
    return output.str();
}

bool stringToUint32(const std::string& text, uint32_t* value)
{
    if (value == NULL || text.empty())
    {
        return false;
    }

    std::istringstream input(text);
    uint32_t parsed = 0;
    input >> parsed;
    if (!input || !input.eof())
    {
        return false;
    }

    *value = parsed;
    return true;
}

void resetCurrentRtpPayload(MediaSessionInfo* session)
{
    if (session == NULL)
    {
        return;
    }

    session->currentRtpPayloadBytes = 0;
    session->currentRtpPayloadPackets = 0;
    session->currentRtpPayloadTimestamp = 0;
    session->currentRtpPayloadData.clear();
    session->hasCurrentRtpPayload = false;
}

} // namespace

bool MediaManager::configure(const NodeConfig& config)
{
    std::queue<int> empty;
    m_availablePorts.swap(empty);
    m_availablePortSet.clear();
    m_allocatedPorts.clear();
    for (std::map<std::string, std::unique_ptr<RtpSessionAdapter> >::iterator adapter = m_rtpSessionAdapters.begin();
         adapter != m_rtpSessionAdapters.end();
         ++adapter)
    {
        adapter->second->stop();
    }
    m_rtpSessionAdapters.clear();
    m_sessions.clear();

    std::set<int> ports;
    const std::vector<SipEndpointConfig>& endpoints = config.sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = endpoints.begin();
         it != endpoints.end();
         ++it)
    {
        int begin = it->rtpPortBegin;
        int end = it->rtpPortEnd;
        if (begin <= 0 || end <= 0 || begin > end)
        {
            continue;
        }

        if (begin % 2 != 0)
        {
            ++begin;
        }
        for (int port = begin; port <= end; port += 2)
        {
            ports.insert(port);
        }
    }

    for (std::set<int>::const_iterator it = ports.begin(); it != ports.end(); ++it)
    {
        m_availablePorts.push(*it);
        m_availablePortSet.insert(*it);
    }

    return !m_availablePorts.empty();
}

int MediaManager::allocateRtpPort()
{
    while (!m_availablePorts.empty())
    {
        const int port = m_availablePorts.front();
        m_availablePorts.pop();

        if (m_availablePortSet.erase(port) == 0)
        {
            continue;
        }

        m_allocatedPorts.insert(port);
        return port;
    }

    return 0;
}

void MediaManager::setFrameSink(std::unique_ptr<MediaFrameSink> sink)
{
    m_frameSink = std::move(sink);
}

void MediaManager::releaseRtpPort(int port)
{
    if (port <= 0)
    {
        return;
    }

    if (m_allocatedPorts.erase(port) > 0 && m_availablePortSet.insert(port).second)
    {
        m_availablePorts.push(port);
    }
}

bool MediaManager::createSession(const MediaSessionInfo& session)
{
    if (session.id.empty() || session.localRtpPort <= 0)
    {
        return false;
    }
    if (m_allocatedPorts.find(session.localRtpPort) == m_allocatedPorts.end())
    {
        return false;
    }

    std::map<std::string, MediaSessionInfo>::iterator existing = m_sessions.find(session.id);
    if (existing != m_sessions.end() && existing->second.localRtpPort != session.localRtpPort)
    {
        releaseRtpPort(existing->second.localRtpPort);
    }

    m_sessions[session.id] = session;
    return true;
}

const MediaSessionInfo* MediaManager::findSession(const std::string& sessionId) const
{
    std::map<std::string, MediaSessionInfo>::const_iterator it = m_sessions.find(sessionId);
    return it == m_sessions.end() ? NULL : &it->second;
}

bool MediaManager::updateSessionState(const std::string& sessionId, const std::string& state)
{
    std::map<std::string, MediaSessionInfo>::iterator it = m_sessions.find(sessionId);
    if (it == m_sessions.end())
    {
        return false;
    }

    it->second.state = state;
    return true;
}

std::vector<MediaSessionInfo> MediaManager::sessionSnapshot() const
{
    std::vector<MediaSessionInfo> sessions;
    for (std::map<std::string, MediaSessionInfo>::const_iterator it = m_sessions.begin();
         it != m_sessions.end();
         ++it)
    {
        sessions.push_back(it->second);
    }
    return sessions;
}

bool MediaManager::startRtpSessionAdapter(const std::string& sessionId,
                                          std::unique_ptr<RtpSessionAdapter> adapter,
                                          std::string* error)
{
    if (!adapter)
    {
        setError(error, "empty RTP session adapter");
        return false;
    }
    std::map<std::string, MediaSessionInfo>::const_iterator session = m_sessions.find(sessionId);
    if (session == m_sessions.end())
    {
        setError(error, "media session not found: " + sessionId);
        return false;
    }
    if (m_rtpSessionAdapters.find(sessionId) != m_rtpSessionAdapters.end())
    {
        setError(error, "RTP session adapter already running: " + sessionId);
        return false;
    }

    RtpSessionOptions options;
    options.sessionId = sessionId;
    options.localRtpPort = session->second.localRtpPort;
    options.remoteIp = session->second.remoteIp;
    options.remoteRtpPort = session->second.remoteRtpPort;
    const bool started = adapter->start(options,
                                        [this](const std::string& callbackSessionId,
                                               const std::vector<unsigned char>& packetBytes) {
                                            std::string ignoredError;
                                            this->receiveRtpPacket(callbackSessionId, packetBytes, &ignoredError);
                                        },
                                        error);
    if (!started)
    {
        return false;
    }

    m_rtpSessionAdapters[sessionId] = std::move(adapter);
    return true;
}

void MediaManager::stopRtpSessionAdapter(const std::string& sessionId)
{
    std::map<std::string, std::unique_ptr<RtpSessionAdapter> >::iterator adapter = m_rtpSessionAdapters.find(sessionId);
    if (adapter == m_rtpSessionAdapters.end())
    {
        return;
    }
    adapter->second->stop();
    m_rtpSessionAdapters.erase(adapter);
}

bool MediaManager::rtpSessionAdapterRunning(const std::string& sessionId) const
{
    std::map<std::string, std::unique_ptr<RtpSessionAdapter> >::const_iterator adapter = m_rtpSessionAdapters.find(sessionId);
    return adapter != m_rtpSessionAdapters.end() && adapter->second && adapter->second->running();
}

bool MediaManager::receiveRtpPacket(const std::string& sessionId,
                                    const std::vector<unsigned char>& packetBytes,
                                    std::string* error)
{
    std::map<std::string, MediaSessionInfo>::iterator it = m_sessions.find(sessionId);
    if (it == m_sessions.end())
    {
        setError(error, "media session not found: " + sessionId);
        return false;
    }

    RtpPacket packet;
    if (!parseRtpPacket(packetBytes, &packet))
    {
        setError(error, "invalid RTP packet");
        return false;
    }

    MediaSessionInfo& session = it->second;
    if (!session.ssrc.empty() && session.ssrc != uint32ToString(packet.ssrc))
    {
        setError(error, "RTP SSRC does not match media session");
        return false;
    }

    if (session.hasRtpSequence)
    {
        const uint16_t expected = static_cast<uint16_t>(session.lastRtpSequence + 1);
        if (packet.sequenceNumber != expected)
        {
            ++session.outOfOrderRtpPackets;
        }
    }

    session.hasRtpSequence = true;
    session.lastRtpSequence = packet.sequenceNumber;
    session.lastRtpTimestamp = packet.timestamp;
    session.lastRtpSsrc = packet.ssrc;
    ++session.receivedRtpPackets;
    session.receivedRtpBytes += packetBytes.size();
    session.receivedPayloadBytes += packet.payload.size();

    if (session.hasCurrentRtpPayload && session.currentRtpPayloadTimestamp != packet.timestamp)
    {
        ++session.incompleteRtpPayloads;
        resetCurrentRtpPayload(&session);
    }
    if (!session.hasCurrentRtpPayload)
    {
        session.currentRtpPayloadTimestamp = packet.timestamp;
        session.hasCurrentRtpPayload = true;
    }
    session.currentRtpPayloadData.insert(session.currentRtpPayloadData.end(), packet.payload.begin(), packet.payload.end());
    session.currentRtpPayloadBytes += packet.payload.size();
    ++session.currentRtpPayloadPackets;

    if (!packet.marker)
    {
        session.state = "stream-receiving";
        return true;
    }

    const std::vector<unsigned char> completePayload = session.currentRtpPayloadData;
    const uint64_t rtpPacketsInFrame = session.currentRtpPayloadPackets;
    resetCurrentRtpPayload(&session);

    ProgramStreamInfo psInfo;
    if (!parseProgramStreamPayload(completePayload, &psInfo))
    {
        setError(error, "invalid PS payload");
        return false;
    }

    uint64_t videoNalUnits = 0;
    uint64_t h264NalUnits = 0;
    uint64_t h265NalUnits = 0;
    uint64_t h264Sps = 0;
    uint64_t h264Pps = 0;
    uint64_t h264Idr = 0;
    uint64_t h265Vps = 0;
    uint64_t h265Sps = 0;
    uint64_t h265Pps = 0;
    uint64_t h265Idr = 0;
    uint64_t videoFrameBytes = 0;
    VideoCodec videoFrameCodec = VIDEO_CODEC_UNKNOWN;
    bool videoFrameKey = false;
    std::vector<unsigned char> videoFrameData;
    for (std::vector<std::vector<unsigned char> >::const_iterator payload = psInfo.videoPayloads.begin();
         payload != psInfo.videoPayloads.end();
         ++payload)
    {
        ElementaryStreamInfo esInfo;
        if (!parseElementaryStream(*payload, &esInfo))
        {
            setError(error, "invalid elementary stream payload");
            return false;
        }
        if (videoFrameCodec == VIDEO_CODEC_UNKNOWN)
        {
            videoFrameCodec = esInfo.codec;
        }
        else if (videoFrameCodec != esInfo.codec)
        {
            setError(error, "mixed video codecs in RTP payload");
            return false;
        }
        videoFrameBytes += payload->size();
        videoFrameData.insert(videoFrameData.end(), payload->begin(), payload->end());
        videoNalUnits += esInfo.nalUnits;
        if (esInfo.codec == VIDEO_CODEC_H264)
        {
            h264NalUnits += esInfo.nalUnits;
            h264Sps += esInfo.h264Sps;
            h264Pps += esInfo.h264Pps;
            h264Idr += esInfo.h264Idr;
            videoFrameKey = videoFrameKey || esInfo.h264Idr > 0;
        }
        else if (esInfo.codec == VIDEO_CODEC_H265)
        {
            h265NalUnits += esInfo.nalUnits;
            h265Vps += esInfo.h265Vps;
            h265Sps += esInfo.h265Sps;
            h265Pps += esInfo.h265Pps;
            h265Idr += esInfo.h265Idr;
            videoFrameKey = videoFrameKey || esInfo.h265Idr > 0;
        }
    }

    session.receivedPsPacks += psInfo.packHeaders;
    session.receivedPesPackets += psInfo.pesPackets;
    session.receivedVideoPesPackets += psInfo.videoPesPackets;
    session.receivedAudioPesPackets += psInfo.audioPesPackets;
    session.receivedPsPayloadBytes += psInfo.pesPayloadBytes;
    session.receivedVideoNalUnits += videoNalUnits;
    session.receivedH264NalUnits += h264NalUnits;
    session.receivedH265NalUnits += h265NalUnits;
    session.receivedH264Sps += h264Sps;
    session.receivedH264Pps += h264Pps;
    session.receivedH264Idr += h264Idr;
    session.receivedH265Vps += h265Vps;
    session.receivedH265Sps += h265Sps;
    session.receivedH265Pps += h265Pps;
    session.receivedH265Idr += h265Idr;
    if (videoFrameBytes > 0)
    {
        session.currentVideoFrameTimestamp = packet.timestamp;
        session.currentVideoFrameCodec = videoFrameCodec;
        session.currentVideoFrameBytes += videoFrameBytes;
        session.currentVideoFramePackets += rtpPacketsInFrame;
        session.currentVideoFrameKey = session.currentVideoFrameKey || videoFrameKey;
        session.currentVideoFrameData.insert(session.currentVideoFrameData.end(), videoFrameData.begin(), videoFrameData.end());
        session.hasCurrentVideoFrame = true;
    }
    if (session.hasCurrentVideoFrame && session.currentVideoFrameBytes > 0)
    {
        if (m_frameSink)
        {
            MediaFrame frame;
            frame.sessionId = session.id;
            frame.peerId = session.peerId;
            frame.codec = session.currentVideoFrameCodec;
            frame.timestamp = session.currentVideoFrameTimestamp;
            frame.packets = session.currentVideoFramePackets;
            frame.keyFrame = session.currentVideoFrameKey;
            frame.data = session.currentVideoFrameData;
            if (!m_frameSink->writeFrame(frame, error))
            {
                return false;
            }
        }
        ++session.completedVideoFrames;
        session.lastVideoFrameBytes = session.currentVideoFrameBytes;
        session.lastVideoFramePackets = session.currentVideoFramePackets;
        session.lastVideoFrameCodec = session.currentVideoFrameCodec;
        session.lastVideoFrameKey = session.currentVideoFrameKey;
        session.currentVideoFrameBytes = 0;
        session.currentVideoFramePackets = 0;
        session.currentVideoFrameCodec = VIDEO_CODEC_UNKNOWN;
        session.currentVideoFrameKey = false;
        session.currentVideoFrameData.clear();
        session.hasCurrentVideoFrame = false;
    }
    session.state = "stream-receiving";
    return true;
}

bool MediaManager::buildOutboundRtpPackets(const std::string& sessionId,
                                           const std::vector<unsigned char>& annexBFrame,
                                           uint16_t firstSequenceNumber,
                                           uint32_t timestamp,
                                           size_t maxPayloadBytes,
                                           std::vector<std::vector<unsigned char> >* packets,
                                           std::string* error) const
{
    if (packets == NULL)
    {
        setError(error, "empty outbound RTP packet output");
        return false;
    }
    if (annexBFrame.empty())
    {
        setError(error, "empty outbound Annex-B frame");
        return false;
    }

    std::map<std::string, MediaSessionInfo>::const_iterator session = m_sessions.find(sessionId);
    if (session == m_sessions.end())
    {
        setError(error, "media session not found: " + sessionId);
        return false;
    }

    uint32_t ssrc = 0;
    if (!stringToUint32(session->second.ssrc, &ssrc))
    {
        setError(error, "invalid outbound RTP SSRC");
        return false;
    }

    RtpPacketizationOptions options;
    options.payloadType = 96;
    options.firstSequenceNumber = firstSequenceNumber;
    options.timestamp = timestamp;
    options.ssrc = ssrc;
    options.maxPayloadBytes = maxPayloadBytes;
    const std::vector<unsigned char> psPayload = buildProgramStreamSample(annexBFrame);
    if (!packetizeRtpPayload(psPayload, options, packets))
    {
        setError(error, "failed to packetize outbound RTP payload");
        return false;
    }

    return true;
}

bool MediaManager::sendAnnexBFrame(const std::string& sessionId,
                                   const std::vector<unsigned char>& annexBFrame,
                                   uint32_t timestampIncrement,
                                   size_t maxPayloadBytes,
                                   std::string* error)
{
    if (annexBFrame.empty())
    {
        setError(error, "empty outbound Annex-B frame");
        return false;
    }
    if (maxPayloadBytes == 0)
    {
        setError(error, "invalid outbound RTP payload size");
        return false;
    }

    std::map<std::string, MediaSessionInfo>::iterator session = m_sessions.find(sessionId);
    if (session == m_sessions.end())
    {
        setError(error, "media session not found: " + sessionId);
        return false;
    }

    std::map<std::string, std::unique_ptr<RtpSessionAdapter> >::iterator adapter = m_rtpSessionAdapters.find(sessionId);
    if (adapter == m_rtpSessionAdapters.end())
    {
        setError(error, "RTP session adapter is not running: " + sessionId);
        return false;
    }

    const std::vector<unsigned char> psPayload = buildProgramStreamSample(annexBFrame);
    size_t offset = 0;
    size_t sentPackets = 0;
    size_t sentPayloadBytes = 0;
    while (offset < psPayload.size())
    {
        const size_t remaining = psPayload.size() - offset;
        const size_t chunkBytes = remaining > maxPayloadBytes ? maxPayloadBytes : remaining;
        RtpPayloadPacket packet;
        packet.payload.assign(psPayload.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset),
                              psPayload.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset + chunkBytes));
        packet.payloadType = 96;
        packet.marker = offset + chunkBytes >= psPayload.size();
        packet.timestampIncrement = packet.marker ? timestampIncrement : 0;
        if (!adapter->second->sendPayloadPacket(packet, error))
        {
            return false;
        }
        ++sentPackets;
        sentPayloadBytes += chunkBytes;
        offset += chunkBytes;
    }

    session->second.sentRtpPayloadPackets += sentPackets;
    session->second.sentRtpPayloadBytes += sentPayloadBytes;
    ++session->second.sentVideoFrames;
    return true;
}

bool MediaManager::closeSession(const std::string& sessionId)
{
    std::map<std::string, MediaSessionInfo>::iterator it = m_sessions.find(sessionId);
    if (it == m_sessions.end())
    {
        return false;
    }

    stopRtpSessionAdapter(sessionId);
    releaseRtpPort(it->second.localRtpPort);
    m_sessions.erase(it);
    return true;
}

size_t MediaManager::availablePortCount() const
{
    return m_availablePortSet.size();
}

size_t MediaManager::sessionCount() const
{
    return m_sessions.size();
}

} // namespace gb28181
