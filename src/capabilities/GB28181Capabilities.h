#ifndef GB28181_CAPABILITIES_GB28181CAPABILITIES_H
#define GB28181_CAPABILITIES_GB28181CAPABILITIES_H

#include "LifecycleCapability.h"
#include "StreamFileFrameSource.h"

#include <ctime>
#include <map>
#include <string>

namespace gb28181 {

struct SipEndpointConfig;

class RegisterClientCapability : public LifecycleCapability
{
public:
    RegisterClientCapability();

protected:
    bool onStart(NodeRuntime& runtime);
};

class RegisterServerCapability : public LifecycleCapability
{
public:
    RegisterServerCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    void onStop();
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);

private:
    struct DigestChallenge
    {
        std::string nonce;
        std::string opaque;
        std::time_t expiresAt;
        unsigned long lastNonceCount;
        bool usedWithoutNonceCount;

        DigestChallenge();
    };

    std::string issueChallenge(const SipRequestContext& request, const SipEndpointConfig& endpoint);
    bool verifyChallenge(const SipRequestContext& request, const SipEndpointConfig& endpoint);
    void cleanupExpiredChallenges(std::time_t now);

    std::map<std::string, DigestChallenge> m_challenges;
};

class KeepaliveClientCapability : public LifecycleCapability
{
public:
    KeepaliveClientCapability();

protected:
    bool onStart(NodeRuntime& runtime);
};

class KeepaliveServerCapability : public LifecycleCapability
{
public:
    KeepaliveServerCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class CatalogClientCapability : public LifecycleCapability
{
public:
    CatalogClientCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class CatalogServerCapability : public LifecycleCapability
{
public:
    CatalogServerCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class RecordQueryClientCapability : public LifecycleCapability
{
public:
    RecordQueryClientCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class RecordQueryServerCapability : public LifecycleCapability
{
public:
    RecordQueryServerCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class InviteClientCapability : public LifecycleCapability
{
public:
    InviteClientCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class InviteServerCapability : public LifecycleCapability
{
public:
    InviteServerCapability();

protected:
    bool onStart(NodeRuntime& runtime);
    bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);
};

class MediaReceiveCapability : public LifecycleCapability
{
public:
    MediaReceiveCapability();
};

class MediaSendCapability : public LifecycleCapability
{
public:
    MediaSendCapability();

protected:
    bool onStart(NodeRuntime& runtime);

private:
    bool sendNextFrame(NodeRuntime& runtime);

    StreamFileFrameSource m_source;
    StreamFileFrame m_pendingFrame;
    bool m_sourceReady;
    bool m_hasPendingFrame;
};

} // namespace gb28181

#endif
