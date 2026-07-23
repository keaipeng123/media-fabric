#include "GB28181Node.h"

#include "SipMessageContext.h"
#include "Logger.h"
#include "SdpSession.h"

#ifdef GB28181_ENABLE_JRTPLIB
#include "JrtplibRtpSessionAdapter.h"
#endif

#include <sstream>
#include <ctime>

namespace gb28181 {
namespace {
std::string nextTransactionCallId(const std::string& prefix, const std::string& localId)
{
    static unsigned long counter = 0;
    std::ostringstream output;
    output << prefix << "-" << std::time(NULL) << "-" << ++counter << "@" << localId;
    return output.str();
}

#ifdef GB28181_ENABLE_JRTPLIB
std::string nextInviteSsrc()
{
    static unsigned long counter = 0;
    const unsigned long value = (static_cast<unsigned long>(std::time(NULL)) + ++counter) % 10000000000UL;
    std::ostringstream output;
    output.width(10);
    output.fill('0');
    output << value;
    return output.str();
}

std::string clientInviteSessionId(const std::string& callId)
{
    return "InviteClientCapability:dialog:" + callId;
}
#endif
}

GB28181Node::GB28181Node(const NodeConfig& config)
    : m_config(config),
      m_runtime(),
      m_started(false)
{
    m_runtime.config = &m_config;
    m_runtime.businessState = &m_businessState;
    m_runtime.sipStack = &m_sipStack;
    m_runtime.peerRegistry = &m_peerRegistry;
    m_runtime.sessionManager = &m_sessionManager;
    m_runtime.taskScheduler = &m_taskScheduler;
    m_runtime.mediaManager = &m_mediaManager;
}

GB28181Node::~GB28181Node()
{
    stop();
}

void GB28181Node::addCapability(std::unique_ptr<Capability> capability)
{
    if (capability)
    {
        m_capabilities.push_back(std::move(capability));
    }
}

bool GB28181Node::start()
{
    if (m_started)
    {
        return true;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "node", "config=" + m_config.configPath());

    if (!m_sipStack.configure(m_config))
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "node", "failed to configure SIP stack");
        return false;
    }
    if (!m_peerRegistry.configure(m_config))
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "node", "failed to configure peer registry");
        return false;
    }
    m_businessState.clear();
    if (!m_mediaManager.configure(m_config))
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "node", "failed to configure media manager");
        return false;
    }
    if (!m_sipStack.start())
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "node", "failed to start SIP stack");
        return false;
    }

    std::ostringstream startup;
    startup << "upstream_peers=" << m_peerRegistry.upstreamCount() << " downstream_peers=" << m_peerRegistry.downstreamCount()
            << " available_rtp_ports=" << m_mediaManager.availablePortCount();
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "node", startup.str());

    for (std::vector<std::unique_ptr<Capability> >::iterator it = m_capabilities.begin();
         it != m_capabilities.end();
         ++it)
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "node", "start capability=" + (*it)->name());
        if (!(*it)->start(m_runtime))
        {
            media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "node", "failed to start capability=" + (*it)->name());
            stop();
            m_sipStack.stop();
            return false;
        }
    }

    m_started = true;
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "node", "sip_routes=" + std::to_string(m_sipStack.routeCount()));
    return true;
}

void GB28181Node::stop()
{
    if (!m_started)
    {
        return;
    }

    m_taskScheduler.stopAll();
    m_mediaManager.stopAllRtpSessionAdapters();

    for (std::vector<std::unique_ptr<Capability> >::reverse_iterator it = m_capabilities.rbegin();
         it != m_capabilities.rend();
         ++it)
    {
        media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "node", "stop capability=" + (*it)->name());
        (*it)->stop();
    }

    m_started = false;
    m_sipStack.stop();
}

bool GB28181Node::dispatchSipRequest(const SipRequestContext& request)
{
    if (!m_started)
    {
        return false;
    }
    return m_sipStack.dispatch(request);
}

std::vector<std::string> GB28181Node::capabilityNames() const
{
    std::vector<std::string> names;
    for (std::vector<std::unique_ptr<Capability> >::const_iterator it = m_capabilities.begin();
         it != m_capabilities.end();
         ++it)
    {
        names.push_back((*it)->name());
    }
    return names;
}

size_t GB28181Node::routeCount() const
{
    return m_sipStack.routeCount();
}

size_t GB28181Node::endpointCount() const
{
    return m_sipStack.endpointCount();
}

size_t GB28181Node::upstreamPeerCount() const
{
    return m_peerRegistry.upstreamCount();
}

size_t GB28181Node::downstreamPeerCount() const
{
    return m_peerRegistry.downstreamCount();
}

size_t GB28181Node::sessionCount() const
{
    return m_sessionManager.sessionCount();
}

size_t GB28181Node::registeredPeerCount() const
{
    return m_peerRegistry.registeredCount();
}

size_t GB28181Node::keepalivePeerCount() const
{
    return m_peerRegistry.keepaliveCount();
}

std::string GB28181Node::peersStatusText() const
{
    std::ostringstream output;
    const std::vector<PeerInfo>& peers = m_peerRegistry.peers();
    for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
    {
        output << it->sipId << " " << (it->relation == PEER_UPSTREAM ? "upstream" : "downstream")
               << " " << it->ip << ":" << it->port
               << " registered=" << (it->registered ? "yes" : "no")
               << " last_register=" << static_cast<long>(it->lastRegisterTime)
               << " last_keepalive=" << static_cast<long>(it->lastKeepaliveTime) << "\n";
    }
    return output.str();
}

std::string GB28181Node::streamsStatusText() const
{
    std::ostringstream output;
    const std::vector<MediaSessionInfo> sessions = m_mediaManager.sessionSnapshot();
    for (std::vector<MediaSessionInfo>::const_iterator session = sessions.begin(); session != sessions.end(); ++session)
    {
        if (session->targetDeviceId.empty())
        {
            continue;
        }
        output << "device=" << session->targetDeviceId
               << " route_peer=" << session->peerId
               << " state=" << session->state
               << " call_id=" << session->callId
               << " local_rtp=" << session->localIp << ":" << session->localRtpPort
               << " remote_rtp=" << session->remoteIp << ":" << session->remoteRtpPort
               << " received_packets=" << session->receivedRtpPackets
               << " received_frames=" << session->completedVideoFrames
               << " received_bytes=" << session->receivedRtpBytes << "\n";
    }
    return output.str();
}

bool GB28181Node::requestRegistration(const std::string& peerId, std::string* error)
{
    PeerInfo* peer = m_peerRegistry.findBySipId(peerId);
    if (peer == NULL || peer->relation != PEER_UPSTREAM) { if(error) *error="unknown upstream peer"; return false; }
    const SipEndpointConfig& local = m_config.node();
    SipMessageContext message; message.method="REGISTER"; message.event="request"; message.fromId=local.sipId; message.toId=peer->sipId;
    message.localIp=local.sipIp; message.localPort=local.sipPort; message.remoteIp=peer->ip; message.remotePort=peer->port; message.expires=peer->registerExpires > 0 ? peer->registerExpires : 3600;
    peer->registrationRequested = true;
    if (!m_sipStack.send(message)) { peer->registrationRequested=false; if(error) *error="failed to send REGISTER"; return false; }
    return true;
}

bool GB28181Node::requestInvite(const std::string& deviceId, std::string* error)
{
    const std::vector<std::string> owners = m_businessState.catalogOwners(deviceId);
    if (owners.empty()) { if (error) *error = "device is not present in synchronized catalog: " + deviceId; return false; }
    if (owners.size() != 1) { if (error) *error = "device belongs to multiple downstream peers: " + deviceId; return false; }

    PeerInfo* peer = m_peerRegistry.findBySipId(owners.front());
    if (peer == NULL || peer->relation != PEER_DOWNSTREAM) { if (error) *error = "catalog owner is not a downstream peer: " + owners.front(); return false; }
    if (!peer->registered) { if (error) *error = "catalog owner is not registered: " + owners.front(); return false; }

#ifndef GB28181_ENABLE_JRTPLIB
    if (error) *error = "real RTP invite requires a build with MEDIA_FABRIC_ENABLE_JRTPLIB=ON";
    return false;
#else
    const SipEndpointConfig& local = m_config.node();
    const int rtpPort = m_mediaManager.allocateRtpPort();
    if (rtpPort <= 0) { if (error) *error = "no RTP port available"; return false; }

    const std::string callId = nextTransactionCallId("invite", local.sipId);
    MediaSessionInfo mediaSession;
    mediaSession.id = clientInviteSessionId(callId);
    mediaSession.peerId = peer->sipId;
    mediaSession.targetDeviceId = deviceId;
    mediaSession.localIp = local.sipIp;
    mediaSession.localRtpPort = rtpPort;
    mediaSession.remoteIp = peer->ip;
    mediaSession.remoteRtpPort = 0;
    mediaSession.transport = "RTP/AVP";
    mediaSession.direction = "recvonly";
    mediaSession.ssrc = nextInviteSsrc();
    mediaSession.callId = callId;
    mediaSession.inviteCseq = "1";
    mediaSession.state = "invite-requested";
    if (!m_mediaManager.createSession(mediaSession))
    {
        m_mediaManager.releaseRtpPort(rtpPort);
        if (error) *error = "failed to create RTP receive session";
        return false;
    }

    std::string rtpError;
    if (!m_mediaManager.startRtpSessionAdapter(mediaSession.id,
                                                std::unique_ptr<RtpSessionAdapter>(new JrtplibRtpSessionAdapter()),
                                                &rtpError))
    {
        m_mediaManager.closeSession(mediaSession.id);
        if (error) *error = "failed to start RTP receiver: " + rtpError;
        return false;
    }

    SipMessageContext message; message.method="INVITE"; message.event="request"; message.fromId=local.sipId; message.toId=deviceId;
    message.localIp=local.sipIp; message.localPort=local.sipPort; message.remoteIp=peer->ip; message.remotePort=peer->port;
    message.callId = callId; message.cseq = mediaSession.inviteCseq; message.contentType="Application/SDP";
    message.body = buildInviteSdp(local.sipId, local.sipIp, rtpPort, mediaSession.ssrc);
    if (!m_sipStack.send(message))
    {
        m_mediaManager.closeSession(mediaSession.id);
        if(error) *error="failed to send INVITE";
        return false;
    }
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "invite",
                                         "sent device=" + deviceId + " route_peer=" + peer->sipId +
                                         " call_id=" + callId + " rtp_port=" + std::to_string(rtpPort));
    return true;
#endif
}

bool GB28181Node::requestBye(const std::string& deviceId, std::string* error)
{
    const std::vector<MediaSessionInfo> sessions = m_mediaManager.sessionSnapshot();
    const MediaSessionInfo* active = NULL;
    for (std::vector<MediaSessionInfo>::const_iterator session = sessions.begin(); session != sessions.end(); ++session)
    {
        if (session->targetDeviceId == deviceId)
        {
            if (active != NULL) { if (error) *error = "multiple active streams for device: " + deviceId; return false; }
            active = &(*session);
        }
    }
    if (active == NULL) { if (error) *error = "no active stream for device: " + deviceId; return false; }
    PeerInfo* peer = m_peerRegistry.findBySipId(active->peerId);
    if (peer == NULL) { if (error) *error = "stream route peer not found: " + active->peerId; return false; }
    const SipEndpointConfig& local = m_config.node();
    SipMessageContext bye;
    bye.method = "BYE"; bye.event = "request"; bye.fromId = local.sipId; bye.toId = deviceId;
    bye.localIp = local.sipIp; bye.localPort = local.sipPort; bye.remoteIp = peer->ip; bye.remotePort = peer->port;
    bye.callId = active->callId; bye.cseq = "2";
    if (!m_sipStack.send(bye)) { if (error) *error = "failed to send BYE"; return false; }
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "invite",
                                         "bye device=" + deviceId + " route_peer=" + peer->sipId +
                                         " call_id=" + active->callId);
    m_mediaManager.closeSession(active->id);
    return true;
}

bool GB28181Node::requestCatalog(const std::string& peerId, std::string* error)
{
    PeerInfo* peer = m_peerRegistry.findBySipId(peerId);
    if (peer == NULL || peer->relation != PEER_DOWNSTREAM)
    {
        if (error) *error = "unknown downstream peer";
        return false;
    }
    if (!peer->registered)
    {
        if (error) *error = "downstream peer is not registered";
        return false;
    }

    const SipEndpointConfig& local = m_config.node();
    SipMessageContext message;
    message.method = "MESSAGE";
    message.event = "Query/Catalog";
    message.fromId = local.sipId;
    message.toId = peer->sipId;
    message.localIp = local.sipIp;
    message.localPort = local.sipPort;
    message.remoteIp = peer->ip;
    message.remotePort = peer->port;
    message.contentType = "Application/MANSCDP+xml";
    message.callId = nextTransactionCallId("catalog-query", local.sipId);
    message.cseq = "1";
    message.body = "<?xml version=\"1.0\"?>\r\n<Query>\r\n<CmdType>Catalog</CmdType>\r\n<SN>1</SN>\r\n<DeviceID>" +
                   peer->sipId + "</DeviceID>\r\n</Query>\r\n";
    if (!m_sipStack.send(message))
    {
        if (error) *error = "failed to send Catalog query";
        return false;
    }
    // A catalog query starts a new snapshot. Subsequent responses are appended one
    // item at a time by CatalogClientCapability.
    m_businessState.updateCatalog(peerId, std::vector<ManscdpItem>());
    return true;
}

size_t GB28181Node::sentSipMessageCount() const
{
    return m_sipStack.sentMessageCount();
}

bool GB28181Node::lastSentSipMessage(SipMessageContext* message) const
{
    return m_sipStack.lastSentMessage(message);
}

size_t GB28181Node::scheduledTaskCount() const
{
    return m_taskScheduler.taskCount();
}

size_t GB28181Node::mediaSessionCount() const
{
    return m_mediaManager.sessionCount();
}

size_t GB28181Node::availableRtpPortCount() const
{
    return m_mediaManager.availablePortCount();
}

void GB28181Node::setMediaFrameSink(std::unique_ptr<MediaFrameSink> sink)
{
    m_mediaManager.setFrameSink(std::move(sink));
}

bool GB28181Node::startRtpSessionAdapter(const std::string& sessionId,
                                         std::unique_ptr<RtpSessionAdapter> adapter,
                                         std::string* error)
{
    return m_mediaManager.startRtpSessionAdapter(sessionId, std::move(adapter), error);
}

void GB28181Node::stopRtpSessionAdapter(const std::string& sessionId)
{
    m_mediaManager.stopRtpSessionAdapter(sessionId);
}

bool GB28181Node::mediaSessionInfo(const std::string& sessionId, MediaSessionInfo* session) const
{
    if (session == NULL)
    {
        return false;
    }
    const MediaSessionInfo* found = m_mediaManager.findSession(sessionId);
    if (found == NULL)
    {
        return false;
    }
    *session = *found;
    return true;
}

bool GB28181Node::receiveRtpPacket(const std::string& sessionId,
                                   const std::vector<unsigned char>& packetBytes,
                                   std::string* error)
{
    return m_mediaManager.receiveRtpPacket(sessionId, packetBytes, error);
}

bool GB28181Node::buildOutboundRtpPackets(const std::string& sessionId,
                                          const std::vector<unsigned char>& annexBFrame,
                                          uint16_t firstSequenceNumber,
                                          uint32_t timestamp,
                                          size_t maxPayloadBytes,
                                          std::vector<std::vector<unsigned char> >* packets,
                                          std::string* error) const
{
    return m_mediaManager.buildOutboundRtpPackets(sessionId,
                                                  annexBFrame,
                                                  firstSequenceNumber,
                                                  timestamp,
                                                  maxPayloadBytes,
                                                  packets,
                                                  error);
}

bool GB28181Node::sendAnnexBFrame(const std::string& sessionId,
                                  const std::vector<unsigned char>& annexBFrame,
                                  uint32_t timestampIncrement,
                                  size_t maxPayloadBytes,
                                  std::string* error)
{
    return m_mediaManager.sendAnnexBFrame(sessionId,
                                          annexBFrame,
                                          timestampIncrement,
                                          maxPayloadBytes,
                                          error);
}

size_t GB28181Node::catalogItemCount() const
{
    return m_businessState.catalogItemCount();
}

size_t GB28181Node::recordItemCount() const
{
    return m_businessState.recordItemCount();
}

std::vector<ManscdpItem> GB28181Node::catalogItems(const std::string& peerId) const
{
    return m_businessState.catalogItems(peerId);
}

std::vector<ManscdpItem> GB28181Node::recordItems(const std::string& peerId) const
{
    return m_businessState.recordItems(peerId);
}

std::string GB28181Node::businessStateSnapshot() const
{
    return m_businessState.serialize();
}

bool GB28181Node::restoreBusinessStateSnapshot(const std::string& snapshot, std::string* error)
{
    return m_businessState.restore(snapshot, error);
}

bool GB28181Node::saveBusinessStateSnapshot(const std::string& path, std::string* error) const
{
    return m_businessState.saveToFile(path, error);
}

bool GB28181Node::loadBusinessStateSnapshot(const std::string& path, std::string* error)
{
    return m_businessState.loadFromFile(path, error);
}

std::string GB28181Node::businessSummaryJson() const
{
    return BusinessQueryService(m_businessState).summaryJson();
}

std::string GB28181Node::catalogJson(const std::string& peerId) const
{
    return BusinessQueryService(m_businessState).catalogJson(peerId);
}

std::string GB28181Node::recordJson(const std::string& peerId) const
{
    return BusinessQueryService(m_businessState).recordJson(peerId);
}

} // namespace gb28181
