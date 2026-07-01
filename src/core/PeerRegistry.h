#ifndef GB28181_CORE_PEERREGISTRY_H
#define GB28181_CORE_PEERREGISTRY_H

#include "NodeConfig.h"

#include <ctime>
#include <string>
#include <vector>

namespace gb28181 {

enum PeerRelation
{
    PEER_UPSTREAM,
    PEER_DOWNSTREAM
};

struct PeerInfo
{
    std::string name;
    std::string sipId;
    std::string ip;
    int port;
    PeerRelation relation;
    bool registered;
    int expires;
    std::time_t lastRegisterTime;
    std::time_t lastKeepaliveTime;

    PeerInfo();
};

class PeerRegistry
{
public:
    bool configure(const NodeConfig& config);

    PeerInfo* findBySipId(const std::string& sipId);
    const PeerInfo* findBySipId(const std::string& sipId) const;
    bool markRegistered(const std::string& sipId, int expires);
    bool markUnregistered(const std::string& sipId);
    bool markKeepalive(const std::string& sipId);

    size_t upstreamCount() const;
    size_t downstreamCount() const;
    size_t registeredCount() const;
    size_t keepaliveCount() const;
    const std::vector<PeerInfo>& peers() const;

private:
    std::vector<PeerInfo> m_peers;
};

} // namespace gb28181

#endif
