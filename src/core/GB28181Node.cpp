#include "GB28181Node.h"

#include <iostream>

namespace gb28181 {

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

    std::cout << "gb28181 node config: " << m_config.configPath() << std::endl;

    if (!m_sipStack.configure(m_config))
    {
        std::cerr << "failed to configure sip stack" << std::endl;
        return false;
    }
    if (!m_peerRegistry.configure(m_config))
    {
        std::cerr << "failed to configure peer registry" << std::endl;
        return false;
    }
    m_businessState.clear();
    if (!m_mediaManager.configure(m_config))
    {
        std::cerr << "failed to configure media manager" << std::endl;
        return false;
    }
    if (!m_sipStack.start())
    {
        std::cerr << "failed to start sip stack" << std::endl;
        return false;
    }

    std::cout << "peer registry: upstream=" << m_peerRegistry.upstreamCount()
              << " downstream=" << m_peerRegistry.downstreamCount() << std::endl;
    std::cout << "media rtp ports available: " << m_mediaManager.availablePortCount() << std::endl;

    for (std::vector<std::unique_ptr<Capability> >::iterator it = m_capabilities.begin();
         it != m_capabilities.end();
         ++it)
    {
        std::cout << "start capability: " << (*it)->name() << std::endl;
        if (!(*it)->start(m_runtime))
        {
            std::cerr << "failed to start capability: " << (*it)->name() << std::endl;
            stop();
            m_sipStack.stop();
            return false;
        }
    }

    m_started = true;
    std::cout << "sip routes registered: " << m_sipStack.routeCount() << std::endl;
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
        std::cout << "stop capability: " << (*it)->name() << std::endl;
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
