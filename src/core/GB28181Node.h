#ifndef GB28181_CORE_GB28181NODE_H
#define GB28181_CORE_GB28181NODE_H

#include "BusinessState.h"
#include "BusinessQueryService.h"
#include "Capability.h"
#include "MediaManager.h"
#include "NodeConfig.h"
#include "NodeRuntime.h"
#include "PeerRegistry.h"
#include "SessionManager.h"
#include "SipRequestContext.h"
#include "SipStack.h"
#include "TaskScheduler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gb28181 {

class GB28181Node
{
public:
    explicit GB28181Node(const NodeConfig& config);
    ~GB28181Node();

    void addCapability(std::unique_ptr<Capability> capability);
    bool start();
    void stop();
    bool dispatchSipRequest(const SipRequestContext& request);

    std::vector<std::string> capabilityNames() const;
    size_t endpointCount() const;
    size_t upstreamPeerCount() const;
    size_t downstreamPeerCount() const;
    size_t routeCount() const;
    size_t sessionCount() const;
    size_t registeredPeerCount() const;
    size_t keepalivePeerCount() const;
    std::string peersStatusText() const;
    std::string streamsStatusText() const;
    bool requestRegistration(const std::string& peerId, std::string* error);
    bool requestInvite(const std::string& deviceId, std::string* error);
    bool requestBye(const std::string& deviceId, std::string* error);
    bool requestCatalog(const std::string& peerId, std::string* error);
    size_t sentSipMessageCount() const;
    bool lastSentSipMessage(SipMessageContext* message) const;
    size_t scheduledTaskCount() const;
    size_t mediaSessionCount() const;
    size_t availableRtpPortCount() const;
    void setMediaFrameSink(std::unique_ptr<MediaFrameSink> sink);
    bool startRtpSessionAdapter(const std::string& sessionId,
                                std::unique_ptr<RtpSessionAdapter> adapter,
                                std::string* error);
    void stopRtpSessionAdapter(const std::string& sessionId);
    bool mediaSessionInfo(const std::string& sessionId, MediaSessionInfo* session) const;
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
    size_t catalogItemCount() const;
    size_t recordItemCount() const;
    std::vector<ManscdpItem> catalogItems(const std::string& peerId) const;
    std::vector<ManscdpItem> recordItems(const std::string& peerId) const;
    std::string businessStateSnapshot() const;
    bool restoreBusinessStateSnapshot(const std::string& snapshot, std::string* error);
    bool saveBusinessStateSnapshot(const std::string& path, std::string* error) const;
    bool loadBusinessStateSnapshot(const std::string& path, std::string* error);
    std::string businessSummaryJson() const;
    std::string catalogJson(const std::string& peerId) const;
    std::string recordJson(const std::string& peerId) const;

private:
    NodeConfig m_config;
    BusinessState m_businessState;
    SipStack m_sipStack;
    PeerRegistry m_peerRegistry;
    SessionManager m_sessionManager;
    TaskScheduler m_taskScheduler;
    MediaManager m_mediaManager;
    NodeRuntime m_runtime;
    std::vector<std::unique_ptr<Capability> > m_capabilities;
    bool m_started;
};

} // namespace gb28181

#endif
