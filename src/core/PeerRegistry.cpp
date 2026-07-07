#include "PeerRegistry.h"

#include <ctime>

namespace gb28181 {

PeerInfo::PeerInfo()
    : port(0),
      relation(PEER_DOWNSTREAM),
      registerTo(false),
      allowRegister(false),
      registerExpires(0),
      registered(false),
      expires(0),
      lastRegisterTime(0),
      lastKeepaliveTime(0)
{
}

bool PeerRegistry::configure(const NodeConfig& config)
{
    m_peers.clear();

    const std::vector<PeerConfig>& peers = config.peers();
    for (std::vector<PeerConfig>::const_iterator it = peers.begin();
         it != peers.end();
         ++it)
    {
        PeerInfo peer;
        peer.name = it->name;
        peer.sipId = it->sipId;
        peer.ip = it->remoteIp;
        peer.port = it->remotePort;
        peer.relation = it->relation == "upstream" ? PEER_UPSTREAM : PEER_DOWNSTREAM;
        peer.registerTo = it->registerTo;
        peer.allowRegister = it->allowRegister;
        peer.registerExpires = it->expires;
        peer.realm = it->realm;
        peer.username = it->username;
        peer.password = it->password;
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

bool PeerRegistry::updateAddress(const std::string& sipId, const std::string& ip, int port)
{
    if (ip.empty() || port <= 0)
    {
        return false;
    }

    PeerInfo* peer = findBySipId(sipId);
    if (peer == NULL)
    {
        return false;
    }

    peer->ip = ip;
    peer->port = port;
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
