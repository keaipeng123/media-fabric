#include "GB28181Capabilities.h"

#include "BusinessState.h"
#include "DigestAuth.h"
#include "NodeRuntime.h"
#include "MediaManager.h"
#include "NodeConfig.h"
#include "PeerRegistry.h"
#include "SessionManager.h"
#include "SipMessageContext.h"
#include "SipStack.h"
#include "StreamFileFrameSource.h"
#include "TaskScheduler.h"
#include "SdpSession.h"

#ifdef GB28181_ENABLE_JRTPLIB
#include "JrtplibRtpSessionAdapter.h"
#endif

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

namespace gb28181 {
namespace {

int nextSn()
{
    static int sn = 1;
    return sn++;
}

int parseInt(const std::string& value)
{
    std::istringstream input(value);
    int result = 0;
    input >> result;
    return input ? result : 0;
}

const int kRegisterExpires = 3600;
const int kRegisterChallengeExpiresSeconds = 300;

const SipEndpointConfig* findEndpointByName(const NodeRuntime& runtime, const std::string& name)
{
    const std::vector<SipEndpointConfig>& endpoints = runtime.config->sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = endpoints.begin(); it != endpoints.end(); ++it)
    {
        if (it->name == name)
        {
            return &(*it);
        }
    }
    return NULL;
}

const SipEndpointConfig* findEndpointBySipId(const NodeRuntime& runtime, const std::string& sipId)
{
    const std::vector<SipEndpointConfig>& endpoints = runtime.config->sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = endpoints.begin(); it != endpoints.end(); ++it)
    {
        if (it->sipId == sipId)
        {
            return &(*it);
        }
    }
    return NULL;
}

const SipEndpointConfig* localEndpointForPeer(const NodeRuntime& runtime, const PeerInfo&)
{
    const SipEndpointConfig* node = findEndpointByName(runtime, "node");
    if (node != NULL)
    {
        return node;
    }

    const std::vector<SipEndpointConfig>& endpoints = runtime.config->sipEndpoints();
    return endpoints.empty() ? NULL : &endpoints.front();
}

bool endpointRequiresAuth(const SipEndpointConfig* endpoint)
{
    return endpoint != NULL && !endpoint->realm.empty() && !endpoint->password.empty();
}

bool matchesManscdp(const SipRequestContext& request, const std::string& root, const std::string& cmdType)
{
    return request.manscdp.root == root && request.manscdp.cmdType == cmdType;
}

bool manscdpDeviceMatchesFrom(const SipRequestContext& request)
{
    return !request.manscdp.deviceId.empty() && request.manscdp.deviceId == request.fromId;
}

bool manscdpTargetsKnownEndpoint(const SipRequestContext& request, const NodeRuntime& runtime)
{
    if (request.manscdp.deviceId.empty())
    {
        return false;
    }

    if (request.manscdp.deviceId == request.toId)
    {
        return true;
    }

    const std::vector<SipEndpointConfig>& endpoints = runtime.config->sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = endpoints.begin(); it != endpoints.end(); ++it)
    {
        if (request.manscdp.deviceId == it->sipId)
        {
            return true;
        }
    }
    return false;
}

bool manscdpListConsistent(const SipRequestContext& request)
{
    return request.manscdp.sumNum <= 0 || request.manscdp.itemCount() > 0;
}

std::string responseState(const std::string& base, const ManscdpMessage& message)
{
    std::ostringstream state;
    state << base << ":sum=" << message.sumNum << ",items=" << message.itemCount();
    if (!message.items.empty() && !message.items.front().deviceId.empty())
    {
        state << ",first=" << message.items.front().deviceId;
    }
    return state.str();
}

std::string makeNonce(const std::string& seed)
{
    return md5Hex("gb28181-node:" + seed);
}

std::string makeRandomNonce(const std::string& seed)
{
    static unsigned long counter = 0;
    std::random_device random;
    std::ostringstream input;
    input << "gb28181-node:" << seed
          << ":" << std::time(NULL)
          << ":" << ++counter
          << ":" << random()
          << ":" << random();
    return md5Hex(input.str());
}

std::string challengeKey(const SipRequestContext& request, const SipEndpointConfig& endpoint)
{
    return request.fromId + ":" + endpoint.sipId + ":" + endpoint.realm;
}

bool parseNonceCount(const std::string& nc, unsigned long* value)
{
    if (nc.empty() || value == NULL)
    {
        return false;
    }

    unsigned long result = 0;
    for (std::string::const_iterator it = nc.begin(); it != nc.end(); ++it)
    {
        const unsigned char c = static_cast<unsigned char>(*it);
        int digit = -1;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = 10 + c - 'a';
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = 10 + c - 'A';
        }
        else
        {
            return false;
        }
        if (result > (ULONG_MAX - static_cast<unsigned long>(digit)) / 16)
        {
            return false;
        }
        result = result * 16 + static_cast<unsigned long>(digit);
    }

    *value = result;
    return true;
}

SipMessageContext makeRequest(const PeerInfo& peer,
                              const SipEndpointConfig& local,
                              const std::string& method,
                              const std::string& event)
{
    SipMessageContext message;
    message.method = method;
    message.event = event;
    message.fromId = local.sipId;
    message.toId = peer.sipId;
    message.localIp = local.sipIp;
    message.localPort = local.sipPort;
    message.remoteIp = peer.ip;
    message.remotePort = peer.port;
    return message;
}

bool parseContactAddress(const std::string& contact, std::string* ip, int* port)
{
    if (ip == NULL || port == NULL)
    {
        return false;
    }

    const std::string marker = "sip:";
    std::string::size_type begin = contact.find(marker);
    if (begin == std::string::npos)
    {
        return false;
    }
    begin += marker.size();

    const std::string::size_type at = contact.find('@', begin);
    if (at == std::string::npos)
    {
        return false;
    }

    std::string::size_type end = contact.find_first_of(">; \t\r\n", at + 1);
    if (end == std::string::npos)
    {
        end = contact.size();
    }

    const std::string hostPort = contact.substr(at + 1, end - at - 1);
    const std::string::size_type colon = hostPort.rfind(':');
    if (colon == std::string::npos)
    {
        return false;
    }

    const std::string parsedIp = hostPort.substr(0, colon);
    const int parsedPort = parseInt(hostPort.substr(colon + 1));
    if (parsedIp.empty() || parsedPort <= 0)
    {
        return false;
    }

    *ip = parsedIp;
    *port = parsedPort;
    return true;
}

std::string sipUri(const std::string& sipId, const std::string& ip, int port)
{
    std::ostringstream output;
    output << "sip:" << sipId << "@" << ip;
    if (port > 0)
    {
        output << ":" << port;
    }
    return output.str();
}

SipMessageContext makeResponse(const SipRequestContext& request,
                               const std::string& capability,
                               int statusCode,
                               const std::string& reason)
{
    SipMessageContext response;
    response.method = request.method;
    response.event = request.event;
    response.fromId = request.toId;
    response.toId = request.fromId;
    response.response = true;
    response.statusCode = statusCode;
    response.reason = reason;
    response.body = capability;
    return response;
}

std::string inviteSessionId(const std::string& capability, const SipRequestContext& request)
{
    if (!request.callId.empty())
    {
        return capability + ":dialog:" + request.callId;
    }
    return capability + ":" + request.fromId;
}

SipMessageContext makeAuthChallengeResponse(const SipRequestContext& request,
                                            const SipEndpointConfig& endpoint,
                                            const std::string& nonce,
                                            const std::string& opaque)
{
    SipMessageContext response = makeResponse(request, "RegisterServerCapability", 401, "Unauthorized");
    response.authRealm = endpoint.realm;
    response.authNonce = nonce;
    response.authOpaque = opaque;
    response.authAlgorithm = "MD5";
    return response;
}

bool verifyRegisterDigestValue(const SipRequestContext& request, const SipEndpointConfig& endpoint)
{
    if (request.digestAuth.response.empty())
    {
        return false;
    }

    const std::string expectedUsername = endpoint.username.empty() ? request.fromId : endpoint.username;
    return verifyDigestResponse(request.method,
                                request.digestAuth,
                                expectedUsername,
                                endpoint.realm,
                                endpoint.password);
}

std::string makeKeepaliveBody(const std::string& deviceId)
{
    std::ostringstream body;
    body << "<?xml version=\"1.0\"?>\r\n"
         << "<Notify>\r\n"
         << "<CmdType>keepalive</CmdType>\r\n"
         << "<SN>" << nextSn() << "</SN>\r\n"
         << "<DeviceID>" << deviceId << "</DeviceID>\r\n"
         << "<Status>OK</Status>\r\n"
         << "</Notify>\r\n";
    return body.str();
}

std::string makeQueryBody(const std::string& cmdType, const std::string& deviceId)
{
    std::ostringstream body;
    body << "<?xml version=\"1.0\"?>\r\n"
         << "<Query>\r\n"
         << "<CmdType>" << cmdType << "</CmdType>\r\n"
         << "<SN>" << nextSn() << "</SN>\r\n"
         << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
    if (cmdType == "RecordInfo")
    {
        body << "<StartTime>1970-01-01T00:00:00</StartTime>\r\n"
             << "<EndTime>1970-01-01T00:00:00</EndTime>\r\n";
    }
    body << "</Query>\r\n";
    return body.str();
}

std::string makeResponseBody(const std::string& root,
                             const std::string& cmdType,
                             const std::string& deviceId,
                             const std::string& result)
{
    std::ostringstream body;
    body << "<?xml version=\"1.0\"?>\r\n"
         << "<" << root << ">\r\n"
         << "<CmdType>" << cmdType << "</CmdType>\r\n"
         << "<SN>" << nextSn() << "</SN>\r\n"
         << "<DeviceID>" << deviceId << "</DeviceID>\r\n"
         << "<Result>" << result << "</Result>\r\n";
    if (root == "Response" && cmdType == "Catalog")
    {
        body << "<SumNum>1</SumNum>\r\n"
             << "<DeviceList Num=\"1\">\r\n"
             << "<Item>\r\n"
             << "<DeviceID>" << deviceId << "</DeviceID>\r\n"
             << "<Name>gb28181-node</Name>\r\n"
             << "<Manufacturer>media-fabric</Manufacturer>\r\n"
             << "<Model>GB28181Node</Model>\r\n"
             << "<Owner>GB28181</Owner>\r\n"
             << "<CivilCode>000000</CivilCode>\r\n"
             << "<Parental>0</Parental>\r\n"
             << "<ParentID>" << deviceId << "</ParentID>\r\n"
             << "<SafetyWay>0</SafetyWay>\r\n"
             << "<RegisterWay>1</RegisterWay>\r\n"
             << "<Secrecy>0</Secrecy>\r\n"
             << "<Status>ON</Status>\r\n"
             << "</Item>\r\n"
             << "</DeviceList>\r\n";
    }
    else if (root == "Response" && cmdType == "RecordInfo")
    {
        body << "<SumNum>1</SumNum>\r\n"
             << "<RecordList Num=\"1\">\r\n"
             << "<Item>\r\n"
             << "<DeviceID>" << deviceId << "</DeviceID>\r\n"
             << "<Name>self-test-record</Name>\r\n"
             << "<FilePath>/tmp/self-test.ps</FilePath>\r\n"
             << "<StartTime>1970-01-01T00:00:00</StartTime>\r\n"
             << "<EndTime>1970-01-01T00:00:00</EndTime>\r\n"
             << "</Item>\r\n"
             << "</RecordList>\r\n";
    }
    body << "</" << root << ">\r\n";
    return body.str();
}

std::string makeInviteBody(const SipEndpointConfig& local)
{
    const int sessionId = static_cast<int>(std::time(NULL));
    return buildInviteSdp(local.sipId,
                          local.sipIp,
                          local.rtpPortBegin > 0 ? local.rtpPortBegin : 30000,
                          std::to_string(sessionId));
}

void createSendSession(NodeRuntime& runtime,
                       const std::string& capability,
                       const PeerInfo& peer,
                       const std::string& state,
                       bool sent)
{
    SessionInfo session;
    session.id = capability + ":send:" + peer.sipId + ":" + state;
    session.capability = capability;
    session.peerId = peer.sipId;
    session.state = sent ? state + "-sent" : state + "-send-failed";
    runtime.sessionManager->createSession(session);
}

bool sendRegister(NodeRuntime& runtime, const std::string& capability, const PeerInfo& peer)
{
    const SipEndpointConfig* local = localEndpointForPeer(runtime, peer);
    if (local == NULL)
    {
        createSendSession(runtime, capability, peer, "register", false);
        return false;
    }

    SipMessageContext message = makeRequest(peer, *local, "REGISTER", "request");
    message.expires = peer.registerExpires > 0 ? peer.registerExpires : kRegisterExpires;
    const bool sent = runtime.sipStack->send(message);
    createSendSession(runtime, capability, peer, "register", sent);
    return sent;
}

bool challengeSupportsAuthQop(const std::string& qop)
{
    return qop.find("auth") != std::string::npos;
}

DigestAuthFields makeRegisterDigest(const SipRequestContext& response,
                                    const PeerInfo& peer,
                                    const SipEndpointConfig& local)
{
    DigestAuthFields digest;
    digest.username = peer.username.empty() ? local.sipId : peer.username;
    digest.realm = response.digestAuth.realm.empty() ? peer.realm : response.digestAuth.realm;
    digest.nonce = response.digestAuth.nonce;
    digest.uri = sipUri(peer.sipId, peer.ip, peer.port);
    digest.algorithm = response.digestAuth.algorithm.empty() ? "MD5" : response.digestAuth.algorithm;
    digest.opaque = response.digestAuth.opaque;
    if (challengeSupportsAuthQop(response.digestAuth.qop))
    {
        digest.qop = "auth";
        digest.nc = "00000001";
        digest.cnonce = makeRandomNonce(local.sipId + ":" + peer.sipId + ":register");
    }
    digest.response = computeDigestResponse("REGISTER",
                                            digest.username,
                                            digest.realm,
                                            peer.password,
                                            digest.nonce,
                                            digest.uri,
                                            digest.qop,
                                            digest.nc,
                                            digest.cnonce);
    return digest;
}

bool sendRegisterWithDigest(NodeRuntime& runtime,
                            const std::string& capability,
                            const PeerInfo& peer,
                            const SipEndpointConfig& local,
                            const SipRequestContext& response)
{
    if (peer.password.empty() || response.digestAuth.nonce.empty())
    {
        createSendSession(runtime, capability, peer, "register-auth", false);
        return false;
    }

    SipMessageContext message = makeRequest(peer, local, "REGISTER", "request");
    message.expires = peer.registerExpires > 0 ? peer.registerExpires : kRegisterExpires;
    message.digestAuth = makeRegisterDigest(response, peer, local);
    const bool sent = runtime.sipStack->send(message);
    createSendSession(runtime, capability, peer, "register-auth", sent);
    return sent;
}

void sendRegisterToUpstreams(NodeRuntime& runtime, const std::string& capability)
{
    const std::vector<PeerInfo>& peers = runtime.peerRegistry->peers();
    for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
    {
        if (it->relation == PEER_UPSTREAM)
        {
            if (it->registrationRequested)
            {
                sendRegister(runtime, capability, *it);
            }
        }
    }
}

const PeerInfo* upstreamPeerForRegisterResponse(NodeRuntime& runtime, const SipRequestContext& response)
{
    const PeerInfo* peer = runtime.peerRegistry->findBySipId(response.toId);
    if (peer != NULL && peer->relation == PEER_UPSTREAM)
    {
        return peer;
    }

    peer = runtime.peerRegistry->findBySipId(response.fromId);
    if (peer != NULL && peer->relation == PEER_UPSTREAM)
    {
        return peer;
    }
    return NULL;
}

const PeerInfo* downstreamPeerForInviteResponse(NodeRuntime& runtime, const SipRequestContext& response)
{
    const PeerInfo* peer = runtime.peerRegistry->findBySipId(response.toId);
    if (peer != NULL && peer->relation == PEER_DOWNSTREAM)
    {
        return peer;
    }

    peer = runtime.peerRegistry->findBySipId(response.fromId);
    if (peer != NULL && peer->relation == PEER_DOWNSTREAM)
    {
        return peer;
    }
    return NULL;
}

bool sendAckForInviteResponse(NodeRuntime& runtime,
                              const std::string& capability,
                              const PeerInfo& peer,
                              const SipRequestContext& response)
{
    const SipEndpointConfig* local = localEndpointForPeer(runtime, peer);
    if (local == NULL)
    {
        createSendSession(runtime, capability, peer, "ack", false);
        return false;
    }

    SipMessageContext ack = makeRequest(peer, *local, "ACK", "request");
    ack.callId = response.callId;
    ack.cseq = response.cseq;
    ack.contact = response.contact;
    ack.fromTag = response.fromTag;
    ack.toTag = response.toTag;
    const bool sent = runtime.sipStack->send(ack);
    createSendSession(runtime, capability, peer, "ack", sent);
    return sent;
}

bool sendKeepalive(NodeRuntime& runtime, const std::string& capability, const PeerInfo& peer)
{
    const SipEndpointConfig* local = localEndpointForPeer(runtime, peer);
    if (local == NULL)
    {
        createSendSession(runtime, capability, peer, "keepalive", false);
        return false;
    }

    SipMessageContext message = makeRequest(peer, *local, "MESSAGE", "Notify/keepalive");
    message.body = makeKeepaliveBody(local->sipId);
    message.contentType = "Application/MANSCDP+xml";
    const bool sent = runtime.sipStack->send(message);
    createSendSession(runtime, capability, peer, "keepalive", sent);
    return sent;
}

void sendKeepaliveToUpstreams(NodeRuntime& runtime, const std::string& capability)
{
    const std::vector<PeerInfo>& peers = runtime.peerRegistry->peers();
    for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
    {
        if (it->relation == PEER_UPSTREAM)
        {
            if (it->registrationRequested && it->registered)
            {
                sendKeepalive(runtime, capability, *it);
            }
        }
    }
}

bool mediaSessionSendable(const MediaSessionInfo& session)
{
    return session.state == "stream-confirmed" || session.state == "stream-receiving";
}

} // namespace

RegisterClientCapability::RegisterClientCapability()
    : LifecycleCapability("RegisterClientCapability")
{
}

bool RegisterClientCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    const std::vector<PeerInfo>& peers = runtime.peerRegistry->peers();
    for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
    {
        if (it->relation != PEER_UPSTREAM)
        {
            continue;
        }

        SessionInfo session;
        session.id = name() + ":intent:" + it->sipId;
        session.capability = name();
        session.peerId = it->sipId;
        session.state = "pending-register";
        runtime.sessionManager->createSession(session);

    }

    if (!registerSipHandler(runtime, "REGISTER", "response"))
    {
        return false;
    }

    const std::string capabilityName = name();
    runtime.taskScheduler->scheduleEvery(name() + ":register-refresh",
                                         runtime.config->timers().registerRefreshSeconds,
                                         [&runtime, capabilityName]() {
                                             sendRegisterToUpstreams(runtime, capabilityName);
                                         });
    return true;
}

bool RegisterClientCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (request.method != "REGISTER" || request.event != "response")
    {
        return false;
    }

    const PeerInfo* peer = upstreamPeerForRegisterResponse(runtime, request);
    if (peer == NULL)
    {
        SessionInfo session;
        session.id = name() + ":response:unknown-peer";
        session.capability = name();
        session.peerId = request.fromId.empty() ? request.toId : request.fromId;
        session.state = "register-response-unknown-peer";
        runtime.sessionManager->createSession(session);
        return false;
    }

    SessionInfo session;
    session.id = name() + ":response:" + peer->sipId;
    session.capability = name();
    session.peerId = peer->sipId;
    std::ostringstream state;
    state << "register-response:" << request.statusCode;
    session.state = state.str();
    runtime.sessionManager->createSession(session);

        if (request.statusCode == 401)
    {
        const SipEndpointConfig* local = localEndpointForPeer(runtime, *peer);
        if (local == NULL)
        {
            createSendSession(runtime, name(), *peer, "register-auth", false);
            return false;
        }
        return sendRegisterWithDigest(runtime, name(), *peer, *local, request);
    }

    if (request.statusCode >= 200 && request.statusCode < 300)
    {
        const int expires = peer->registerExpires > 0 ? peer->registerExpires : kRegisterExpires;
        return runtime.peerRegistry->markRegistered(peer->sipId, expires);
    }

    return request.statusCode > 0 && request.statusCode < 400;
}

RegisterServerCapability::DigestChallenge::DigestChallenge()
    : expiresAt(0),
      lastNonceCount(0),
      usedWithoutNonceCount(false)
{
}

RegisterServerCapability::RegisterServerCapability()
    : LifecycleCapability("RegisterServerCapability")
{
}

bool RegisterServerCapability::onStart(NodeRuntime& runtime)
{
    return LifecycleCapability::onStart(runtime) &&
           registerSipHandler(runtime, "REGISTER", "request");
}

void RegisterServerCapability::onStop()
{
    m_challenges.clear();
    LifecycleCapability::onStop();
}

std::string RegisterServerCapability::issueChallenge(const SipRequestContext& request, const SipEndpointConfig& endpoint)
{
    cleanupExpiredChallenges(std::time(NULL));

    DigestChallenge challenge;
    challenge.nonce = makeRandomNonce(challengeKey(request, endpoint));
    challenge.opaque = makeNonce(endpoint.sipId + ":" + endpoint.realm);
    challenge.expiresAt = std::time(NULL) + kRegisterChallengeExpiresSeconds;
    m_challenges[challengeKey(request, endpoint)] = challenge;
    return challenge.nonce;
}

bool RegisterServerCapability::verifyChallenge(const SipRequestContext& request, const SipEndpointConfig& endpoint)
{
    cleanupExpiredChallenges(std::time(NULL));

    const std::string key = challengeKey(request, endpoint);
    std::map<std::string, DigestChallenge>::iterator it = m_challenges.find(key);
    if (it == m_challenges.end())
    {
        return false;
    }

    DigestChallenge& challenge = it->second;
    if (request.digestAuth.nonce != challenge.nonce)
    {
        return false;
    }

    unsigned long nonceCount = 0;
    if (!request.digestAuth.qop.empty())
    {
        if (request.digestAuth.cnonce.empty() ||
            !parseNonceCount(request.digestAuth.nc, &nonceCount) ||
            nonceCount <= challenge.lastNonceCount)
        {
            return false;
        }
    }
    else if (challenge.usedWithoutNonceCount)
    {
        return false;
    }

    if (!verifyRegisterDigestValue(request, endpoint))
    {
        return false;
    }

    if (!request.digestAuth.qop.empty())
    {
        challenge.lastNonceCount = nonceCount;
    }
    else
    {
        challenge.usedWithoutNonceCount = true;
    }
    return true;
}

void RegisterServerCapability::cleanupExpiredChallenges(std::time_t now)
{
    for (std::map<std::string, DigestChallenge>::iterator it = m_challenges.begin(); it != m_challenges.end();)
    {
        if (it->second.expiresAt > 0 && it->second.expiresAt <= now)
        {
            it = m_challenges.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool RegisterServerCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    cleanupExpiredChallenges(std::time(NULL));

    if (request.fromId.empty())
    {
        return false;
    }

    const PeerInfo* peer = runtime.peerRegistry->findBySipId(request.fromId);
    if (peer == NULL)
    {
        SessionInfo session;
        session.id = name() + ":" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "unknown-peer";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 403, "Forbidden");
        runtime.sipStack->send(response);
        return false;
    }
    if (peer->relation == PEER_DOWNSTREAM && !peer->allowRegister)
    {
        SessionInfo session;
        session.id = name() + ":register-not-allowed:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "register-not-allowed";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 403, "Forbidden");
        runtime.sipStack->send(response);
        return false;
    }

    const SipEndpointConfig* targetEndpoint = findEndpointBySipId(runtime, request.toId);
    if (endpointRequiresAuth(targetEndpoint) && request.digestAuth.response.empty())
    {
        SessionInfo session;
        session.id = name() + ":auth:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "auth-challenge";
        runtime.sessionManager->createSession(session);

        const std::string nonce = issueChallenge(request, *targetEndpoint);
        const std::string key = challengeKey(request, *targetEndpoint);
        const std::map<std::string, DigestChallenge>::const_iterator challenge = m_challenges.find(key);
        const std::string opaque = challenge == m_challenges.end() ? makeNonce(targetEndpoint->sipId) : challenge->second.opaque;
        SipMessageContext response = makeAuthChallengeResponse(request, *targetEndpoint, nonce, opaque);
        runtime.sipStack->send(response);
        return true;
    }
    if (endpointRequiresAuth(targetEndpoint) && !verifyChallenge(request, *targetEndpoint))
    {
        SessionInfo session;
        session.id = name() + ":auth-failed:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "auth-failed";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 403, "Forbidden");
        runtime.sipStack->send(response);
        return false;
    }

    bool ok = false;
    std::string state;
    if (request.expires == 0)
    {
        ok = runtime.peerRegistry->markUnregistered(request.fromId);
        state = ok ? "unregistered" : "unknown-peer";
    }
    else
    {
        ok = runtime.peerRegistry->markRegistered(request.fromId, request.expires);
        std::string contactIp;
        int contactPort = 0;
        if (ok && parseContactAddress(request.contact, &contactIp, &contactPort))
        {
            runtime.peerRegistry->updateAddress(request.fromId, contactIp, contactPort);
        }
        state = ok ? "registered" : "unknown-peer";
    }

    SessionInfo session;
    session.id = name() + ":" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = state;
    runtime.sessionManager->createSession(session);

    SipMessageContext response = makeResponse(request, name(), ok ? 200 : 403, ok ? "OK" : "Forbidden");
    runtime.sipStack->send(response);
    return ok;
}

KeepaliveClientCapability::KeepaliveClientCapability()
    : LifecycleCapability("KeepaliveClientCapability")
{
}

bool KeepaliveClientCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    const std::string capabilityName = name();
    runtime.taskScheduler->scheduleEvery(name() + ":keepalive",
                                         runtime.config->timers().heartbeatIntervalSeconds,
                                         [&runtime, capabilityName]() {
                                             sendKeepaliveToUpstreams(runtime, capabilityName);
                                         });
    return true;
}

KeepaliveServerCapability::KeepaliveServerCapability()
    : LifecycleCapability("KeepaliveServerCapability")
{
}

bool KeepaliveServerCapability::onStart(NodeRuntime& runtime)
{
    return LifecycleCapability::onStart(runtime) &&
           registerSipHandler(runtime, "MESSAGE", "Notify/keepalive");
}

bool KeepaliveServerCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (!matchesManscdp(request, "Notify", "keepalive") || !manscdpDeviceMatchesFrom(request))
    {
        SessionInfo session;
        session.id = name() + ":bad-xml:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "bad-xml";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 400, "Bad Request");
        runtime.sipStack->send(response);
        return false;
    }

    const bool knownPeer = runtime.peerRegistry->markKeepalive(request.fromId);

    SessionInfo session;
    session.id = name() + ":" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = knownPeer ? "keepalive" : "unknown-peer";
    runtime.sessionManager->createSession(session);

    SipMessageContext response = makeResponse(request, name(), knownPeer ? 200 : 403, knownPeer ? "OK" : "Forbidden");
    runtime.sipStack->send(response);
    return knownPeer;
}

CatalogClientCapability::CatalogClientCapability()
    : LifecycleCapability("CatalogClientCapability")
{
}

bool CatalogClientCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    const std::string capabilityName = name();
    auto sendCatalog = [&runtime, capabilityName]() {
        const std::vector<PeerInfo>& peers = runtime.peerRegistry->peers();
        for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
        {
            if (it->relation != PEER_DOWNSTREAM)
            {
                continue;
            }

            const SipEndpointConfig* local = localEndpointForPeer(runtime, *it);
            if (local == NULL)
            {
                createSendSession(runtime, capabilityName, *it, "catalog-query", false);
                continue;
            }

            SipMessageContext message = makeRequest(*it, *local, "MESSAGE", "Query/Catalog");
            message.body = makeQueryBody("Catalog", it->sipId);
            message.contentType = "Application/MANSCDP+xml";
            const bool sent = runtime.sipStack->send(message);
            createSendSession(runtime, capabilityName, *it, "catalog-query", sent);
        }
    };

    sendCatalog();
    runtime.taskScheduler->scheduleEvery(name() + ":catalog-retry", 5, sendCatalog);

    return registerSipHandler(runtime, "MESSAGE", "Response/Catalog");
}

bool CatalogClientCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (!matchesManscdp(request, "Response", "Catalog") || !manscdpDeviceMatchesFrom(request) || !manscdpListConsistent(request))
    {
        SessionInfo session;
        session.id = name() + ":bad-response:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "bad-xml";
        runtime.sessionManager->createSession(session);
        return false;
    }

    SessionInfo session;
    session.id = name() + ":response:" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = responseState("catalog-response-received", request.manscdp);
    runtime.sessionManager->createSession(session);
    runtime.businessState->updateCatalog(request.fromId, request.manscdp.items);
    return true;
}

CatalogServerCapability::CatalogServerCapability()
    : LifecycleCapability("CatalogServerCapability")
{
}

bool CatalogServerCapability::onStart(NodeRuntime& runtime)
{
    return LifecycleCapability::onStart(runtime) &&
           registerSipHandler(runtime, "MESSAGE", "Query/Catalog");
}

bool CatalogServerCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (!matchesManscdp(request, "Query", "Catalog") || !manscdpTargetsKnownEndpoint(request, runtime))
    {
        SessionInfo session;
        session.id = name() + ":bad-query:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "bad-xml";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 400, "Bad Request");
        runtime.sipStack->send(response);
        return false;
    }

    SessionInfo session;
    session.id = name() + ":" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = "catalog-query-received";
    runtime.sessionManager->createSession(session);

    SipMessageContext response = makeResponse(request, name(), 200, "OK");
    runtime.sipStack->send(response);

    const PeerInfo* peer = runtime.peerRegistry->findBySipId(request.fromId);
    if (peer != NULL)
    {
        const SipEndpointConfig* local = localEndpointForPeer(runtime, *peer);
        if (local != NULL)
        {
            SipMessageContext catalog = makeRequest(*peer, *local, "MESSAGE", "Response/Catalog");
            catalog.body = makeResponseBody("Response", "Catalog", local->sipId, "OK");
            catalog.contentType = "Application/MANSCDP+xml";
            runtime.sipStack->send(catalog);
        }
    }
    return true;
}

RecordQueryClientCapability::RecordQueryClientCapability()
    : LifecycleCapability("RecordQueryClientCapability")
{
}

bool RecordQueryClientCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    const std::string capabilityName = name();
    auto sendRecordQuery = [&runtime, capabilityName]() {
        const std::vector<PeerInfo>& peers = runtime.peerRegistry->peers();
        for (std::vector<PeerInfo>::const_iterator it = peers.begin(); it != peers.end(); ++it)
        {
            if (it->relation != PEER_DOWNSTREAM)
            {
                continue;
            }

            const SipEndpointConfig* local = localEndpointForPeer(runtime, *it);
            if (local == NULL)
            {
                createSendSession(runtime, capabilityName, *it, "record-query", false);
                continue;
            }

            SipMessageContext message = makeRequest(*it, *local, "MESSAGE", "Query/RecordInfo");
            message.body = makeQueryBody("RecordInfo", it->sipId);
            message.contentType = "Application/MANSCDP+xml";
            const bool sent = runtime.sipStack->send(message);
            createSendSession(runtime, capabilityName, *it, "record-query", sent);
        }
    };

    sendRecordQuery();
    runtime.taskScheduler->scheduleEvery(name() + ":record-retry", 5, sendRecordQuery);

    return registerSipHandler(runtime, "MESSAGE", "Response/RecordInfo");
}

bool RecordQueryClientCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (!matchesManscdp(request, "Response", "RecordInfo") || !manscdpDeviceMatchesFrom(request) || !manscdpListConsistent(request))
    {
        SessionInfo session;
        session.id = name() + ":bad-response:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "bad-xml";
        runtime.sessionManager->createSession(session);
        return false;
    }

    SessionInfo session;
    session.id = name() + ":response:" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = responseState("record-response-received", request.manscdp);
    runtime.sessionManager->createSession(session);
    runtime.businessState->updateRecords(request.fromId, request.manscdp.items);
    return true;
}

RecordQueryServerCapability::RecordQueryServerCapability()
    : LifecycleCapability("RecordQueryServerCapability")
{
}

bool RecordQueryServerCapability::onStart(NodeRuntime& runtime)
{
    return LifecycleCapability::onStart(runtime) &&
           registerSipHandler(runtime, "MESSAGE", "Query/RecordInfo");
}

bool RecordQueryServerCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    if (!matchesManscdp(request, "Query", "RecordInfo") || !manscdpTargetsKnownEndpoint(request, runtime))
    {
        SessionInfo session;
        session.id = name() + ":bad-query:" + request.fromId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "bad-xml";
        runtime.sessionManager->createSession(session);

        SipMessageContext response = makeResponse(request, name(), 400, "Bad Request");
        runtime.sipStack->send(response);
        return false;
    }

    SessionInfo session;
    session.id = name() + ":" + request.fromId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = "record-query-received";
    runtime.sessionManager->createSession(session);

    SipMessageContext response = makeResponse(request, name(), 200, "OK");
    runtime.sipStack->send(response);

    const PeerInfo* peer = runtime.peerRegistry->findBySipId(request.fromId);
    if (peer != NULL)
    {
        const SipEndpointConfig* local = localEndpointForPeer(runtime, *peer);
        if (local != NULL)
        {
            SipMessageContext record = makeRequest(*peer, *local, "MESSAGE", "Response/RecordInfo");
            record.body = makeResponseBody("Response", "RecordInfo", local->sipId, "OK");
            record.contentType = "Application/MANSCDP+xml";
            runtime.sipStack->send(record);
        }
    }
    return true;
}

InviteClientCapability::InviteClientCapability()
    : LifecycleCapability("InviteClientCapability")
{
}

bool InviteClientCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    return registerSipHandler(runtime, "INVITE", "response");
}

bool InviteClientCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    const PeerInfo* peer = downstreamPeerForInviteResponse(runtime, request);

    SessionInfo session;
    session.id = name() + ":response:" + (peer == NULL ? request.fromId : peer->sipId);
    session.capability = name();
    session.peerId = peer == NULL ? request.fromId : peer->sipId;
    std::ostringstream state;
    state << "invite-response:" << request.statusCode;
    session.state = state.str();
    runtime.sessionManager->createSession(session);

    if (request.statusCode >= 200 && request.statusCode < 300)
    {
        if (peer == NULL)
        {
            return false;
        }
        return sendAckForInviteResponse(runtime, name(), *peer, request);
    }

    return true;
}

InviteServerCapability::InviteServerCapability()
    : LifecycleCapability("InviteServerCapability")
{
}

bool InviteServerCapability::onStart(NodeRuntime& runtime)
{
    return LifecycleCapability::onStart(runtime) &&
           registerSipHandler(runtime, "INVITE", "request") &&
           registerSipHandler(runtime, "ACK", "request") &&
           registerSipHandler(runtime, "BYE", "request");
}

bool InviteServerCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    const std::string sessionId = inviteSessionId(name(), request);
    if (request.method == "ACK")
    {
        const MediaSessionInfo* mediaSession = runtime.mediaManager->findSession(sessionId);
        if (mediaSession == NULL)
        {
            return false;
        }
        if (!mediaSession->inviteCseq.empty() && !request.cseq.empty() && mediaSession->inviteCseq != request.cseq)
        {
            return false;
        }
#ifdef GB28181_ENABLE_JRTPLIB
        if (!runtime.mediaManager->rtpSessionAdapterRunning(sessionId))
        {
            std::string rtpError;
            if (!runtime.mediaManager->startRtpSessionAdapter(
                    sessionId,
                    std::unique_ptr<RtpSessionAdapter>(new JrtplibRtpSessionAdapter()),
                    &rtpError))
            {
                runtime.mediaManager->updateSessionState(sessionId, "rtp-start-failed:" + rtpError);
                return false;
            }

            std::cout << "rtp session started: " << sessionId
                      << " local=" << mediaSession->localIp << ":" << mediaSession->localRtpPort
                      << " remote=" << mediaSession->remoteIp << ":" << mediaSession->remoteRtpPort
                      << std::endl;
        }
#endif
        if (!runtime.mediaManager->updateSessionState(sessionId, "stream-confirmed"))
        {
            return false;
        }

        SessionInfo session;
        session.id = sessionId;
        session.capability = name();
        session.peerId = request.fromId;
        session.state = "stream-confirmed";
        runtime.sessionManager->createSession(session);
        return true;
    }

    if (request.method == "BYE")
    {
        if (!runtime.mediaManager->closeSession(sessionId))
        {
            SipMessageContext response = makeResponse(request, name(), 481, "Call/Transaction Does Not Exist");
            runtime.sipStack->send(response);
            return false;
        }
        runtime.sessionManager->closeSession(sessionId);

        SessionInfo byeSession;
        byeSession.id = name() + ":BYE:" + request.fromId;
        byeSession.capability = name();
        byeSession.peerId = request.fromId;
        byeSession.state = "stream-stopped";
        runtime.sessionManager->createSession(byeSession);

        SipMessageContext response = makeResponse(request, name(), 200, "OK");
        runtime.sipStack->send(response);
        return true;
    }

    const int rtpPort = runtime.mediaManager->allocateRtpPort();
    if (rtpPort <= 0)
    {
        return false;
    }

    SdpInfo requestSdp;
    if (!parseSdp(request.body, &requestSdp))
    {
        runtime.mediaManager->releaseRtpPort(rtpPort);

        SessionInfo badSdpSession;
        badSdpSession.id = name() + ":bad-sdp:" + request.fromId;
        badSdpSession.capability = name();
        badSdpSession.peerId = request.fromId;
        badSdpSession.state = "bad-sdp";
        runtime.sessionManager->createSession(badSdpSession);

        SipMessageContext response = makeResponse(request, name(), 400, "Bad Request");
        runtime.sipStack->send(response);
        return false;
    }

    std::ostringstream state;
    state << "stream-requested:local_rtp=" << rtpPort
          << ",remote=" << requestSdp.connectionIp << ":" << requestSdp.mediaPort
          << ",transport=" << requestSdp.transport;
    if (!requestSdp.ssrc.empty())
    {
        state << ",ssrc=" << requestSdp.ssrc;
    }

    SessionInfo session;
    session.id = sessionId;
    session.capability = name();
    session.peerId = request.fromId;
    session.state = state.str();
    runtime.sessionManager->createSession(session);

    SipMessageContext response = makeResponse(request, name(), 200, "OK");
    const PeerInfo* peer = runtime.peerRegistry->findBySipId(request.fromId);
    const SipEndpointConfig* local = peer != NULL ? localEndpointForPeer(runtime, *peer) : findEndpointBySipId(runtime, request.toId);
    MediaSessionInfo mediaSession;
    mediaSession.id = sessionId;
    mediaSession.peerId = request.fromId;
    mediaSession.localIp = local != NULL ? local->sipIp : requestSdp.connectionIp;
    mediaSession.localRtpPort = rtpPort;
    mediaSession.remoteIp = requestSdp.connectionIp;
    mediaSession.remoteRtpPort = requestSdp.mediaPort;
    mediaSession.transport = requestSdp.transport;
    mediaSession.direction = requestSdp.direction;
    mediaSession.ssrc = requestSdp.ssrc;
    mediaSession.callId = request.callId;
    mediaSession.inviteCseq = request.cseq;
    mediaSession.remoteContact = request.contact;
    mediaSession.state = "stream-requested";
    if (!runtime.mediaManager->createSession(mediaSession))
    {
        runtime.sessionManager->closeSession(sessionId);
        runtime.mediaManager->releaseRtpPort(rtpPort);
        SipMessageContext error = makeResponse(request, name(), 500, "Internal Server Error");
        runtime.sipStack->send(error);
        return false;
    }
    response.body = buildInviteResponseSdp(request.toId,
                                           mediaSession.localIp,
                                           rtpPort,
                                           requestSdp);
    response.contentType = "Application/SDP";
    runtime.sipStack->send(response);
    return true;
}

MediaReceiveCapability::MediaReceiveCapability()
    : LifecycleCapability("MediaReceiveCapability")
{
}

MediaSendCapability::MediaSendCapability()
    : LifecycleCapability("MediaSendCapability"),
      m_source(),
      m_pendingFrame(),
      m_sourceReady(false),
      m_hasPendingFrame(false)
{
}

bool MediaSendCapability::onStart(NodeRuntime& runtime)
{
    if (!LifecycleCapability::onStart(runtime))
    {
        return false;
    }

    const MediaConfig& media = runtime.config->media();
    SessionInfo session;
    session.id = name() + ":source";
    session.capability = name();
    session.peerId = "";
    if (media.streamFile.empty())
    {
        session.state = "media-source-not-configured";
        runtime.sessionManager->createSession(session);
        return true;
    }

    std::string error;
    StreamFileFrame frame;
    if (!m_source.open(media.streamFile, &error) || !m_source.readNextVideoFrame(&frame, &error))
    {
        session.state = "media-source-error:" + error;
        runtime.sessionManager->createSession(session);
        return true;
    }

    m_sourceReady = true;
    m_pendingFrame = frame;
    m_hasPendingFrame = true;

    std::ostringstream state;
    state << "media-source-ready:path=" << media.streamFile
          << ",pts=" << frame.pts
          << ",bytes=" << frame.data.size()
          << ",key=" << (frame.keyFrame ? 1 : 0)
          << ",payload_bytes=" << media.rtpPayloadBytes
          << ",timestamp_increment=" << media.rtpTimestampIncrement
          << ",send_interval_ms=" << media.streamSendIntervalMs
          << ",loop=" << (media.streamLoop ? 1 : 0);
    session.state = state.str();
    runtime.sessionManager->createSession(session);

    const std::string capabilityName = name();
    runtime.taskScheduler->scheduleEveryMs(name() + ":media-send",
                                           media.streamSendIntervalMs,
                                           [this, &runtime, capabilityName]() {
                                               (void)capabilityName;
                                               this->sendNextFrame(runtime);
                                           });
    sendNextFrame(runtime);
    return true;
}

bool MediaSendCapability::sendNextFrame(NodeRuntime& runtime)
{
    if (!m_sourceReady)
    {
        return false;
    }

    const MediaConfig& media = runtime.config->media();
    if (!m_hasPendingFrame)
    {
        std::string readError;
        if (!m_source.readNextVideoFrame(&m_pendingFrame, &readError))
        {
            if (media.streamLoop && m_source.reopen(&readError) && m_source.readNextVideoFrame(&m_pendingFrame, &readError))
            {
                SessionInfo loopSession;
                loopSession.id = name() + ":source:loop";
                loopSession.capability = name();
                std::ostringstream loopState;
                loopState << "media-source-loop:path=" << media.streamFile
                          << ",pts=" << m_pendingFrame.pts
                          << ",bytes=" << m_pendingFrame.data.size();
                loopSession.state = loopState.str();
                runtime.sessionManager->createSession(loopSession);
                m_hasPendingFrame = true;
            }
            else
            {
                SessionInfo session;
                session.id = name() + ":source:eof";
                session.capability = name();
                session.state = "media-source-eof:" + readError;
                runtime.sessionManager->createSession(session);
                return false;
            }
        }
        else
        {
            m_hasPendingFrame = true;
        }
        if (!m_hasPendingFrame)
        {
            SessionInfo session;
            session.id = name() + ":source:eof";
            session.capability = name();
            session.state = "media-source-eof:" + readError;
            runtime.sessionManager->createSession(session);
            return false;
        }
    }

    size_t sentSessions = 0;
    std::string lastError;
    const std::vector<MediaSessionInfo> sessions = runtime.mediaManager->sessionSnapshot();
    for (std::vector<MediaSessionInfo>::const_iterator session = sessions.begin();
         session != sessions.end();
         ++session)
    {
        if (!mediaSessionSendable(*session) || !runtime.mediaManager->rtpSessionAdapterRunning(session->id))
        {
            continue;
        }

        std::string sendError;
        if (runtime.mediaManager->sendAnnexBFrame(session->id,
                                                  m_pendingFrame.data,
                                                  media.rtpTimestampIncrement,
                                                  media.rtpPayloadBytes,
                                                  &sendError))
        {
            ++sentSessions;
        }
        else
        {
            lastError = sendError;
        }
    }

    if (sentSessions == 0)
    {
        return false;
    }

    SessionInfo session;
    session.id = name() + ":frame:" + std::to_string(m_pendingFrame.pts);
    session.capability = name();
    std::ostringstream state;
    state << "media-frame-sent:pts=" << m_pendingFrame.pts
          << ",bytes=" << m_pendingFrame.data.size()
          << ",sessions=" << sentSessions;
    if (!lastError.empty())
    {
        state << ",last_error=" << lastError;
    }
    session.state = state.str();
    runtime.sessionManager->createSession(session);
    m_hasPendingFrame = false;
    return true;
}

} // namespace gb28181
