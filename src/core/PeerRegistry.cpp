#include "PeerRegistry.h"

#include <ctime>

namespace gb28181 {

PeerInfo::PeerInfo()
    : port(0),
      relation(PEER_DOWNSTREAM),
      registered(false),
      expires(0),
      lastRegisterTime(0),
      lastKeepaliveTime(0)
{
}

bool PeerRegistry::configure(const NodeConfig& config)
{
    m_peers.clear();

    const std::vector<SipEndpointConfig>& endpoints = config.sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = endpoints.begin();
         it != endpoints.end();
         ++it)
    {
        PeerInfo peer;
        peer.name = it->name;
        peer.sipId = it->sipId;
        peer.ip = it->sipIp;
        peer.port = it->sipPort;
        peer.relation = it->name == "sup" ? PEER_UPSTREAM : PEER_DOWNSTREAM;
        peer.registered = false;
        peer.expires = 0;
        peer.lastRegisterTime = 0;
        peer.lastKeepaliveTime = 0;
        m_peers.push_back(peer);
    }

    return true;
}

PeerInfo* PeerRegistry::findBySipId(const std::string& sipId)
{
    for (std::vector<PeerInfo>::iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->sipId == sipId)
        {
            return &(*it);
        }
    }
    return NULL;
}

const PeerInfo* PeerRegistry::findBySipId(const std::string& sipId) const
{
    for (std::vector<PeerInfo>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->sipId == sipId)
        {
            return &(*it);
        }
    }
    return NULL;
}

bool PeerRegistry::markRegistered(const std::string& sipId, int expires)
{
    PeerInfo* peer = findBySipId(sipId);
    if (peer == NULL)
    {
        return false;
    }

    peer->registered = true;
    peer->expires = expires;
    peer->lastRegisterTime = std::time(NULL);
    return true;
}

bool PeerRegistry::markUnregistered(const std::string& sipId)
{
    PeerInfo* peer = findBySipId(sipId);
    if (peer == NULL)
    {
        return false;
    }

    peer->registered = false;
    peer->expires = 0;
    peer->lastRegisterTime = 0;
    peer->lastKeepaliveTime = 0;
    return true;
}

bool PeerRegistry::markKeepalive(const std::string& sipId)
{
    PeerInfo* peer = findBySipId(sipId);
    if (peer == NULL)
    {
        return false;
    }

    peer->lastKeepaliveTime = std::time(NULL);
    return true;
}

size_t PeerRegistry::upstreamCount() const
{
    size_t count = 0;
    for (std::vector<PeerInfo>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->relation == PEER_UPSTREAM)
        {
            ++count;
        }
    }
    return count;
}

size_t PeerRegistry::downstreamCount() const
{
    size_t count = 0;
    for (std::vector<PeerInfo>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->relation == PEER_DOWNSTREAM)
        {
            ++count;
        }
    }
    return count;
}

size_t PeerRegistry::registeredCount() const
{
    size_t count = 0;
    for (std::vector<PeerInfo>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->registered)
        {
            ++count;
        }
    }
    return count;
}

size_t PeerRegistry::keepaliveCount() const
{
    size_t count = 0;
    for (std::vector<PeerInfo>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it)
    {
        if (it->lastKeepaliveTime > 0)
        {
            ++count;
        }
    }
    return count;
}

const std::vector<PeerInfo>& PeerRegistry::peers() const
{
    return m_peers;
}

} // namespace gb28181
