#include "GB28181Node.h"
#include "DigestAuth.h"
#include "ManscdpMessage.h"
#include "MediaFrameSink.h"
#include "ManagementServer.h"
#include "MediaFabricRuntime.h"
#include "Logger.h"
#include "NodeConfig.h"
#include "ProgramStream.h"
#include "RtpPacket.h"
#include "RtpSessionAdapter.h"
#include "SdpSession.h"
#include "SipMessageContext.h"
#include "SipRequestContext.h"
#include "StandardCapabilities.h"
#include "StreamFileFrameSource.h"

#include <cstdio>
#include <atomic>
#include <csignal>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::atomic<bool> g_serverStopSignal(false);
std::mutex g_serverStopMutex;
std::condition_variable g_serverStopCondition;

void onServerSignal(int)
{
    std::lock_guard<std::mutex> lock(g_serverStopMutex);
    g_serverStopSignal.store(true);
    g_serverStopCondition.notify_all();
}

void waitForServerStopSignal()
{
    std::signal(SIGINT, onServerSignal);
    std::signal(SIGTERM, onServerSignal);
    std::unique_lock<std::mutex> lock(g_serverStopMutex);
    g_serverStopCondition.wait(lock, [] { return g_serverStopSignal.load(); });
}

const char* kDefaultConfigPath = "conf/media-fabric.conf";
const char* kSelfTestUpstreamId = "10000000002000000001";
const char* kSelfTestNodeId = "11000000002000000001";
const char* kSelfTestDownstreamId = "12000000002000000001";
const char* kSelfTestNodeIp = "127.0.0.1";
const int kSelfTestNodePort = 7101;
const char* kSelfTestUpstreamIp = "127.0.0.1";
const int kSelfTestUpstreamPort = 5061;
const char* kSelfTestRealm = "1000000000";
const char* kSelfTestUsername = "admin";
const char* kSelfTestPassword = "admin";
const char* kSelfTestBusinessStatePath = "/tmp/gb28181-business-state-self-test.snapshot";
const char* kSelfTestFramePath = "/tmp/gb28181-self-test-frame.h264";

class SelfTestRtpSessionAdapter : public gb28181::RtpSessionAdapter
{
public:
    SelfTestRtpSessionAdapter(const std::vector<std::vector<unsigned char> >& packets,
                              std::vector<gb28181::RtpPayloadPacket>* sentPackets)
        : m_packets(packets),
          m_sentPackets(sentPackets),
          m_running(false)
    {
    }

    bool start(const gb28181::RtpSessionOptions& options,
               gb28181::RtpPacketCallback callback,
               std::string* error)
    {
        if (options.sessionId.empty() || options.localRtpPort <= 0 || !callback)
        {
            if (error)
            {
                *error = "invalid self-test RTP adapter options";
            }
            return false;
        }

        m_running = true;
        for (std::vector<std::vector<unsigned char> >::const_iterator packet = m_packets.begin();
             packet != m_packets.end();
             ++packet)
        {
            callback(options.sessionId, *packet);
        }
        return true;
    }

    bool sendPayloadPacket(const gb28181::RtpPayloadPacket& packet, std::string* error)
    {
        if (!m_running)
        {
            if (error)
            {
                *error = "self-test RTP adapter is not running";
            }
            return false;
        }
        if (packet.payload.empty())
        {
            if (error)
            {
                *error = "empty self-test RTP payload packet";
            }
            return false;
        }
        if (m_sentPackets)
        {
            m_sentPackets->push_back(packet);
        }
        return true;
    }

    void stop()
    {
        m_running = false;
    }

    bool running() const
    {
        return m_running;
    }

private:
    std::vector<std::vector<unsigned char> > m_packets;
    std::vector<gb28181::RtpPayloadPacket>* m_sentPackets;
    bool m_running;
};

void printUsage(const char* programName)
{
    std::cout << "Usage: " << programName << " [-c config_path] [--self-test]" << std::endl
              << "       " << programName
              << " [-c config_path] --business-query summary|catalog|record --business-state-file path [--peer-id sip_id]"
              << std::endl;
}

std::string selfTestNodeRegisterUri()
{
    return std::string("sip:") + kSelfTestNodeId + "@" + kSelfTestNodeIp + ":" + std::to_string(kSelfTestNodePort);
}

std::string selfTestUpstreamRegisterUri()
{
    return std::string("sip:") + kSelfTestUpstreamId + "@" + kSelfTestUpstreamIp + ":" + std::to_string(kSelfTestUpstreamPort);
}

void fillRegisterDigest(gb28181::SipRequestContext* request,
                        const std::string& nonce,
                        const std::string& response,
                        const std::string& qop,
                        const std::string& nc,
                        const std::string& cnonce)
{
    request->digestAuth.username = kSelfTestUsername;
    request->digestAuth.realm = kSelfTestRealm;
    request->digestAuth.nonce = nonce;
    request->digestAuth.uri = selfTestNodeRegisterUri();
    request->digestAuth.response = response;
    request->digestAuth.algorithm = "MD5";
    request->digestAuth.qop = qop;
    request->digestAuth.nc = nc;
    request->digestAuth.cnonce = cnonce;
}

void fillManscdp(gb28181::SipRequestContext* request, const std::string& body)
{
    request->body = body;
    if (gb28181::parseManscdpMessage(body, &request->manscdp))
    {
        request->event = request->manscdp.event();
    }
}

std::string makeSelfTestKeepaliveXml(const std::string& deviceId)
{
    return "<?xml version=\"1.0\"?>\r\n"
           "<Notify>\r\n"
           "<CmdType>keepalive</CmdType>\r\n"
           "<SN>100</SN>\r\n"
           "<DeviceID>" +
           deviceId +
           "</DeviceID>\r\n"
           "<Status>OK</Status>\r\n"
           "</Notify>\r\n";
}

std::string makeSelfTestQueryXml(const std::string& cmdType, const std::string& deviceId)
{
    std::string body = "<?xml version=\"1.0\"?>\r\n"
                       "<Query>\r\n"
                       "<CmdType>" +
                       cmdType +
                       "</CmdType>\r\n"
                       "<SN>101</SN>\r\n"
                       "<DeviceID>" +
                       deviceId +
                       "</DeviceID>\r\n";
    if (cmdType == "RecordInfo")
    {
        body += "<StartTime>1970-01-01T00:00:00</StartTime>\r\n"
                "<EndTime>1970-01-01T00:00:00</EndTime>\r\n";
    }
    body += "</Query>\r\n";
    return body;
}

std::string makeSelfTestResponseXml(const std::string& cmdType, const std::string& deviceId)
{
    std::string body = "<?xml version=\"1.0\"?>\r\n"
                       "<Response>\r\n"
                       "<CmdType>" +
                       cmdType +
                       "</CmdType>\r\n"
                       "<SN>102</SN>\r\n"
                       "<DeviceID>" +
                       deviceId +
                       "</DeviceID>\r\n"
                       "<Result>OK</Result>\r\n"
                       "<SumNum>1</SumNum>\r\n";
    if (cmdType == "Catalog")
    {
        body += "<DeviceList Num=\"1\">\r\n"
                "<Item>\r\n"
                "<DeviceID>" +
                deviceId +
                "</DeviceID>\r\n"
                "<Name>SelfTestCamera</Name>\r\n"
                "<Manufacturer>media-fabric</Manufacturer>\r\n"
                "<Model>SelfTestModel</Model>\r\n"
                "<Owner>SelfTestOwner</Owner>\r\n"
                "<CivilCode>110000</CivilCode>\r\n"
                "<Parental>1</Parental>\r\n"
                "<ParentID>10000000002000000001</ParentID>\r\n"
                "<SafetyWay>0</SafetyWay>\r\n"
                "<RegisterWay>1</RegisterWay>\r\n"
                "<Secrecy>0</Secrecy>\r\n"
                "<Status>ON</Status>\r\n"
                "</Item>\r\n"
                "</DeviceList>\r\n";
    }
    else if (cmdType == "RecordInfo")
    {
        body += "<RecordList Num=\"1\">\r\n"
                "<Item>\r\n"
                "<DeviceID>" +
                deviceId +
                "</DeviceID>\r\n"
                "<Name>SelfTestRecord</Name>\r\n"
                "<FilePath>/tmp/self-test.ps</FilePath>\r\n"
                "<StartTime>1970-01-01T00:00:00</StartTime>\r\n"
                "<EndTime>1970-01-01T00:00:00</EndTime>\r\n"
                "</Item>\r\n"
                "</RecordList>\r\n";
    }
    body += "</Response>\r\n";
    return body;
}

std::string makeSelfTestBadResponseXml(const std::string& cmdType, const std::string& deviceId)
{
    return "<?xml version=\"1.0\"?>\r\n"
           "<Response>\r\n"
           "<CmdType>" +
           cmdType +
           "</CmdType>\r\n"
           "<SN>103</SN>\r\n"
           "<DeviceID>" +
           deviceId +
           "</DeviceID>\r\n"
           "<Result>OK</Result>\r\n"
           "<SumNum>1</SumNum>\r\n"
           "</Response>\r\n";
}

bool queryBusinessJson(const gb28181::GB28181Node& node,
                       const std::string& query,
                       const std::string& peerId,
                       std::string* json,
                       std::string* error)
{
    if (json == NULL)
    {
        if (error)
        {
            *error = "empty business query output";
        }
        return false;
    }

    if (query == "summary")
    {
        *json = node.businessSummaryJson();
        return true;
    }
    if (query == "catalog")
    {
        if (peerId.empty())
        {
            if (error)
            {
                *error = "catalog business query requires --peer-id";
            }
            return false;
        }
        *json = node.catalogJson(peerId);
        return true;
    }
    if (query == "record")
    {
        if (peerId.empty())
        {
            if (error)
            {
                *error = "record business query requires --peer-id";
            }
            return false;
        }
        *json = node.recordJson(peerId);
        return true;
    }

    if (error)
    {
        *error = "unknown business query: " + query;
    }
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string configPath = kDefaultConfigPath;
    bool selfTest = false;
    std::string businessQuery;
    std::string businessStateFile;
    std::string businessQueryPeerId;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-c" || arg == "--config")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            configPath = argv[++i];
            continue;
        }
        if (arg == "--self-test")
        {
            selfTest = true;
            continue;
        }
        if (arg == "--business-query")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            businessQuery = argv[++i];
            continue;
        }
        if (arg == "--business-state-file")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            businessStateFile = argv[++i];
            continue;
        }
        if (arg == "--peer-id")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            businessQueryPeerId = argv[++i];
            continue;
        }

        std::cerr << "unknown argument: " << arg << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    gb28181::NodeConfig config;
    if (!config.load(configPath))
    {
        std::cerr << "failed to load config: " << configPath << std::endl;
        return 1;
    }
    media_fabric::Logger::instance().configure(media_fabric::LOG_INFO, "media-fabric.log", 10 * 1024 * 1024, 5);

    gb28181::GB28181Node node(config);
    if (!businessQuery.empty())
    {
        if (selfTest)
        {
            std::cerr << "--business-query cannot be combined with --self-test" << std::endl;
            return 1;
        }
        if (businessStateFile.empty())
        {
            std::cerr << "--business-query requires --business-state-file" << std::endl;
            return 1;
        }

        std::string businessError;
        if (!node.loadBusinessStateSnapshot(businessStateFile, &businessError))
        {
            std::cerr << businessError << std::endl;
            return 1;
        }

        std::string json;
        if (!queryBusinessJson(node, businessQuery, businessQueryPeerId, &json, &businessError))
        {
            std::cerr << businessError << std::endl;
            return 1;
        }

        std::cout << json << std::endl;
        return 0;
    }

    // The standalone executable remains available for protocol diagnosis, but
    // production lifecycle management is shared with the Go host through this
    // same embeddable runtime.
    if (!selfTest)
    {
        gb28181::MediaFabricRuntime runtime;
        std::string runtimeError;
        if (!runtime.start(configPath, true, &runtimeError))
        {
            std::cerr << runtimeError << std::endl;
            return 1;
        }
        waitForServerStopSignal();
        runtime.stop();
        return 0;
    }

    if (selfTest)
    {
        std::remove(kSelfTestFramePath);
        node.setMediaFrameSink(std::unique_ptr<gb28181::MediaFrameSink>(
            new gb28181::FileMediaFrameSink(kSelfTestFramePath)));
    }

    std::vector<std::unique_ptr<gb28181::Capability> > capabilities = gb28181::createStandardCapabilities();
    for (std::vector<std::unique_ptr<gb28181::Capability> >::iterator it = capabilities.begin();
         it != capabilities.end();
         ++it)
    {
        node.addCapability(std::move(*it));
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "starting with config " + configPath);
    if (!node.start())
    {
        return 1;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "started");
    gb28181::ManagementServer managementServer;
    if (!selfTest)
    {
        std::string managementError;
        if (!managementServer.start(config.managementSocketPath(), [&node](const std::string& request) {
                std::istringstream input(request); std::string command, peerId; input >> command >> peerId;
                if (command == "peers") return std::string("OK\n") + node.peersStatusText();
                std::string error;
                if (command == "register") return node.requestRegistration(peerId, &error) ? std::string("OK REGISTER sent\n") : std::string("ERROR ") + error + "\n";
                if (command == "invite") return node.requestInvite(peerId, &error) ? std::string("OK INVITE sent\n") : std::string("ERROR ") + error + "\n";
                if (command == "bye") return node.requestBye(peerId, &error) ? std::string("OK BYE sent\n") : std::string("ERROR ") + error + "\n";
                if (command == "streams") return std::string("OK\n") + node.streamsStatusText();
                if (command == "catalog-query") return node.requestCatalog(peerId, &error) ? std::string("OK Catalog query sent\n") : std::string("ERROR ") + error + "\n";
                if (command == "catalog-show") return std::string("OK\n") + node.catalogJson(peerId) + "\n";
                if (command == "help") return std::string("OK commands: peers, register <peer-id>, invite <catalog-device-id>, bye <catalog-device-id>, streams, catalog-query <peer-id>, catalog-show <peer-id>\n");
                return std::string("ERROR unknown command\n");
            }, &managementError))
        {
            media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "management", "startup failed: " + managementError);
            node.stop(); return 1;
        }
    }
    if (selfTest)
    {
        const bool singleEndpointOk = node.endpointCount() == 1;
        const bool peerTopologyOk = node.upstreamPeerCount() == 1 && node.downstreamPeerCount() == 1;
        const size_t initialRtpPorts = node.availableRtpPortCount();
        std::string mediaSourceError;
        gb28181::StreamFileFrameSource mediaSource;
        gb28181::StreamFileFrame mediaSourceFrame;
        const bool mediaSourceOk =
            !config.media().streamFile.empty() &&
            config.media().rtpPayloadBytes == 1300 &&
            config.media().rtpTimestampIncrement == 3600 &&
            mediaSource.open(config.media().streamFile, &mediaSourceError) &&
            mediaSource.readNextVideoFrame(&mediaSourceFrame, &mediaSourceError) &&
            mediaSourceFrame.type == 2 &&
            mediaSourceFrame.pts >= 0 &&
            !mediaSourceFrame.data.empty();
        gb28181::ProgramStreamInfo mediaSourcePsInfo;
        const std::vector<unsigned char> mediaSourcePsPayload =
            mediaSourceOk ? gb28181::buildProgramStreamSample(mediaSourceFrame.data) : std::vector<unsigned char>();
        const bool mediaSourcePsOk =
            mediaSourceOk &&
            gb28181::parseProgramStreamPayload(mediaSourcePsPayload, &mediaSourcePsInfo) &&
            mediaSourcePsInfo.videoPayloads.size() == 1 &&
            mediaSourcePsInfo.videoPayloads.front() == mediaSourceFrame.data;

        gb28181::SipRequestContext regChallenge;
        regChallenge.method = "REGISTER";
        regChallenge.event = "request";
        regChallenge.fromId = kSelfTestDownstreamId;
        regChallenge.toId = kSelfTestNodeId;
        regChallenge.expires = 60;

        gb28181::SipRequestContext registerClientChallenge;
        registerClientChallenge.method = "REGISTER";
        registerClientChallenge.event = "response";
        registerClientChallenge.fromId = kSelfTestUpstreamId;
        registerClientChallenge.toId = kSelfTestNodeId;
        registerClientChallenge.statusCode = 401;
        registerClientChallenge.reason = "Unauthorized";
        registerClientChallenge.digestAuth.realm = kSelfTestRealm;
        registerClientChallenge.digestAuth.nonce = "register-client-nonce";
        registerClientChallenge.digestAuth.opaque = "register-client-opaque";
        registerClientChallenge.digestAuth.algorithm = "MD5";

        gb28181::SipRequestContext invite;
        invite.method = "INVITE";
        invite.event = "request";
        invite.fromId = kSelfTestUpstreamId;
        invite.toId = kSelfTestNodeId;
        invite.callId = "selftest-invite-dialog";
        invite.cseq = "1";
        invite.contact = "<sip:10000000002000000001@127.0.0.1:5061>";
        invite.body = gb28181::buildInviteSdp(kSelfTestNodeId, kSelfTestNodeIp, 30000, "1234567890");

        gb28181::SipRequestContext badInvite = invite;
        badInvite.body = "v=0\r\ns=Bad\r\n";

        gb28181::SipRequestContext ack;
        ack.method = "ACK";
        ack.event = "request";
        ack.fromId = kSelfTestUpstreamId;
        ack.toId = kSelfTestNodeId;
        ack.callId = invite.callId;
        ack.cseq = invite.cseq;

        gb28181::SipRequestContext badAckCseq = ack;
        badAckCseq.cseq = "2";

        gb28181::SipRequestContext keepalive;
        keepalive.method = "MESSAGE";
        keepalive.fromId = kSelfTestDownstreamId;
        keepalive.toId = kSelfTestNodeId;
        fillManscdp(&keepalive, makeSelfTestKeepaliveXml(kSelfTestDownstreamId));

        gb28181::SipRequestContext badKeepalive = keepalive;
        fillManscdp(&badKeepalive, makeSelfTestKeepaliveXml("00000000000000000000"));

        gb28181::SipRequestContext catalog;
        catalog.method = "MESSAGE";
        catalog.fromId = kSelfTestUpstreamId;
        catalog.toId = kSelfTestNodeId;
        fillManscdp(&catalog, makeSelfTestQueryXml("Catalog", kSelfTestNodeId));

        gb28181::SipRequestContext record;
        record.method = "MESSAGE";
        record.fromId = kSelfTestUpstreamId;
        record.toId = kSelfTestNodeId;
        fillManscdp(&record, makeSelfTestQueryXml("RecordInfo", kSelfTestNodeId));

        gb28181::SipRequestContext catalogResponse;
        catalogResponse.method = "MESSAGE";
        catalogResponse.fromId = kSelfTestDownstreamId;
        catalogResponse.toId = kSelfTestNodeId;
        fillManscdp(&catalogResponse, makeSelfTestResponseXml("Catalog", kSelfTestDownstreamId));
        // Catalog responses are delivered in multiple MESSAGE requests, one item at
        // a time.  Both messages report the same total item count.
        catalogResponse.manscdp.sumNum = 2;
        gb28181::SipRequestContext catalogResponseSecond = catalogResponse;
        catalogResponseSecond.manscdp.items.front().deviceId = "12000000001310000059";
        catalogResponseSecond.manscdp.items.front().name = "SelfTestCameraTwo";

        gb28181::SipRequestContext badCatalogResponse = catalogResponse;
        fillManscdp(&badCatalogResponse, makeSelfTestBadResponseXml("Catalog", kSelfTestDownstreamId));

        gb28181::SipRequestContext recordResponse;
        recordResponse.method = "MESSAGE";
        recordResponse.fromId = kSelfTestDownstreamId;
        recordResponse.toId = kSelfTestNodeId;
        fillManscdp(&recordResponse, makeSelfTestResponseXml("RecordInfo", kSelfTestDownstreamId));

        gb28181::SipRequestContext badRecordResponse = recordResponse;
        fillManscdp(&badRecordResponse, makeSelfTestBadResponseXml("RecordInfo", kSelfTestDownstreamId));

        gb28181::SipRequestContext bye;
        bye.method = "BYE";
        bye.event = "request";
        bye.fromId = kSelfTestUpstreamId;
        bye.toId = kSelfTestNodeId;
        bye.callId = invite.callId;
        bye.cseq = "3";

        gb28181::SipRequestContext wrongDialogBye = bye;
        wrongDialogBye.callId = "missing-dialog";

        gb28181::SipRequestContext inviteResponse;
        inviteResponse.method = "INVITE";
        inviteResponse.event = "response";
        inviteResponse.fromId = kSelfTestDownstreamId;
        inviteResponse.toId = kSelfTestNodeId;
        inviteResponse.callId = "selftest-invite-client-dialog";
        inviteResponse.cseq = "1";
        inviteResponse.contact = "<sip:12000000002000000001@127.0.0.1:7201>";
        inviteResponse.fromTag = "selftest-local-tag";
        inviteResponse.toTag = "selftest-remote-tag";
        inviteResponse.statusCode = 200;
        inviteResponse.reason = "OK";

        const bool regChallengeOk = node.dispatchSipRequest(regChallenge);
        gb28181::SipMessageContext challengeMessage;
        const bool challengeCaptured = node.lastSentSipMessage(&challengeMessage);
        const std::string challengeNonce = challengeMessage.authNonce;
        const bool challengeNonceOk = challengeCaptured &&
                                      challengeMessage.response &&
                                      challengeMessage.statusCode == 401 &&
                                      !challengeNonce.empty();
        const bool registerAuthRetryDispatched = node.dispatchSipRequest(registerClientChallenge);
        gb28181::SipMessageContext registerAuthMessage;
        const bool registerAuthMessageCaptured = node.lastSentSipMessage(&registerAuthMessage);
        const bool registerAuthRetryOk =
            registerAuthRetryDispatched &&
            registerAuthMessageCaptured &&
            !registerAuthMessage.response &&
            registerAuthMessage.method == "REGISTER" &&
            registerAuthMessage.digestAuth.username == kSelfTestUsername &&
            registerAuthMessage.digestAuth.realm == kSelfTestRealm &&
            registerAuthMessage.digestAuth.nonce == registerClientChallenge.digestAuth.nonce &&
            registerAuthMessage.digestAuth.uri == selfTestUpstreamRegisterUri() &&
            registerAuthMessage.digestAuth.opaque == registerClientChallenge.digestAuth.opaque &&
            gb28181::verifyDigestResponse("REGISTER",
                                          registerAuthMessage.digestAuth,
                                          kSelfTestUsername,
                                          kSelfTestRealm,
                                          kSelfTestPassword);

        gb28181::SipRequestContext reg;
        reg.method = "REGISTER";
        reg.event = "request";
        reg.fromId = kSelfTestDownstreamId;
        reg.toId = kSelfTestNodeId;
        reg.expires = 60;
        fillRegisterDigest(&reg,
                           challengeNonce,
                           gb28181::computeDigestResponse(reg.method,
                                                          kSelfTestUsername,
                                                          kSelfTestRealm,
                                                          kSelfTestPassword,
                                                          challengeNonce,
                                                          selfTestNodeRegisterUri(),
                                                          "",
                                                          "",
                                                          ""),
                           "",
                           "",
                           "");

        gb28181::SipRequestContext badReg = reg;
        fillRegisterDigest(&badReg,
                           challengeNonce,
                           "00000000000000000000000000000000",
                           "",
                           "",
                           "");

        const bool badRegRejected = !node.dispatchSipRequest(badReg);
        const bool regOk = node.dispatchSipRequest(reg);
        const bool replayRegRejected = !node.dispatchSipRequest(reg);

        const bool qopChallengeOk = node.dispatchSipRequest(regChallenge);
        gb28181::SipMessageContext qopChallengeMessage;
        const bool qopChallengeCaptured = node.lastSentSipMessage(&qopChallengeMessage);
        const std::string qopNonce = qopChallengeMessage.authNonce;
        const bool qopChallengeNonceOk = qopChallengeCaptured &&
                                         qopChallengeMessage.response &&
                                         qopChallengeMessage.statusCode == 401 &&
                                         !qopNonce.empty();

        gb28181::SipRequestContext qopReg = reg;
        fillRegisterDigest(&qopReg,
                           qopNonce,
                           gb28181::computeDigestResponse(qopReg.method,
                                                          kSelfTestUsername,
                                                          kSelfTestRealm,
                                                          kSelfTestPassword,
                                                          qopNonce,
                                                          selfTestNodeRegisterUri(),
                                                          "auth",
                                                          "00000001",
                                                          "selftest-cnonce"),
                           "auth",
                           "00000001",
                           "selftest-cnonce");
        const bool qopRegOk = node.dispatchSipRequest(qopReg);
        const bool qopReplayRejected = !node.dispatchSipRequest(qopReg);

        const bool badKeepaliveRejected = !node.dispatchSipRequest(badKeepalive);
        const bool keepaliveOk = node.dispatchSipRequest(keepalive);
        const bool catalogOk = node.dispatchSipRequest(catalog);
        const bool recordOk = node.dispatchSipRequest(record);
        const bool badCatalogResponseRejected = !node.dispatchSipRequest(badCatalogResponse);
        const bool catalogResponseOk = node.dispatchSipRequest(catalogResponse);
        const bool catalogResponseSecondOk = node.dispatchSipRequest(catalogResponseSecond);
        gb28181::BusinessState catalogRoutingState;
        catalogRoutingState.updateCatalog(kSelfTestDownstreamId, catalogResponse.manscdp.items);
        const bool catalogRouteResolved = catalogRoutingState.catalogOwners(kSelfTestDownstreamId).size() == 1 &&
                                          catalogRoutingState.catalogOwners(kSelfTestDownstreamId).front() == kSelfTestDownstreamId;
        catalogRoutingState.updateCatalog("12000000002000000002", catalogResponse.manscdp.items);
        const bool catalogRouteAmbiguous = catalogRoutingState.catalogOwners(kSelfTestDownstreamId).size() == 2;
        const bool catalogRouteMissing = catalogRoutingState.catalogOwners("00000000000000000000").empty();
        const bool badRecordResponseRejected = !node.dispatchSipRequest(badRecordResponse);
        const bool recordResponseOk = node.dispatchSipRequest(recordResponse);
        const bool inviteResponseOk = node.dispatchSipRequest(inviteResponse);
        gb28181::SipMessageContext inviteAckMessage;
        const bool inviteAckCaptured = node.lastSentSipMessage(&inviteAckMessage);
        const bool inviteAckOk =
            inviteResponseOk &&
            inviteAckCaptured &&
            !inviteAckMessage.response &&
            inviteAckMessage.method == "ACK" &&
            inviteAckMessage.fromId == kSelfTestNodeId &&
            inviteAckMessage.toId == kSelfTestDownstreamId &&
            inviteAckMessage.callId == inviteResponse.callId &&
            inviteAckMessage.cseq == inviteResponse.cseq &&
            inviteAckMessage.contact == inviteResponse.contact &&
            inviteAckMessage.fromTag == inviteResponse.fromTag &&
            inviteAckMessage.toTag == inviteResponse.toTag;
        const bool badInviteRejected = !node.dispatchSipRequest(badInvite);
        const bool earlyAckRejected = !node.dispatchSipRequest(ack);
        const bool inviteOk = node.dispatchSipRequest(invite);
        const bool badAckCseqRejected = !node.dispatchSipRequest(badAckCseq);
        const bool ackOk = node.dispatchSipRequest(ack);
        const std::string mediaSessionId = std::string("InviteServerCapability:dialog:") + invite.callId;
        const unsigned char videoPayloadBytes1[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x21};
        const unsigned char videoPayloadBytes2[] = {0x00, 0x00, 0x00, 0x01, 0x41, 0x9a, 0x22, 0x11};
        const std::vector<unsigned char> videoPayload1(videoPayloadBytes1, videoPayloadBytes1 + sizeof(videoPayloadBytes1));
        const std::vector<unsigned char> videoPayload2(videoPayloadBytes2, videoPayloadBytes2 + sizeof(videoPayloadBytes2));
        std::vector<unsigned char> videoFramePayload;
        videoFramePayload.insert(videoFramePayload.end(), videoPayload1.begin(), videoPayload1.end());
        videoFramePayload.insert(videoFramePayload.end(), videoPayload2.begin(), videoPayload2.end());
        const std::vector<unsigned char> psFramePayload = gb28181::buildProgramStreamSample(videoFramePayload);
        std::string mediaError;
        const std::vector<unsigned char> badRtpPacket =
            gb28181::buildRtpPacket(96, true, 7, 90000, 1, psFramePayload);
        const bool badRtpSsrcRejected = !node.receiveRtpPacket(mediaSessionId, badRtpPacket, &mediaError);
        std::vector<std::vector<unsigned char> > outboundRtpPackets;
        const bool outboundPacketizeOk = node.buildOutboundRtpPackets(mediaSessionId,
                                                                      videoFramePayload,
                                                                      7,
                                                                      90000,
                                                                      20,
                                                                      &outboundRtpPackets,
                                                                      &mediaError);
        bool rtpPacketizeOk = false;
        if (outboundPacketizeOk && outboundRtpPackets.size() == 2)
        {
            gb28181::RtpPacket firstPacket;
            gb28181::RtpPacket secondPacket;
            std::vector<unsigned char> reassembledPayload;
            const bool firstPacketOk = gb28181::parseRtpPacket(outboundRtpPackets[0], &firstPacket);
            const bool secondPacketOk = gb28181::parseRtpPacket(outboundRtpPackets[1], &secondPacket);
            if (firstPacketOk && secondPacketOk)
            {
                reassembledPayload.insert(reassembledPayload.end(), firstPacket.payload.begin(), firstPacket.payload.end());
                reassembledPayload.insert(reassembledPayload.end(), secondPacket.payload.begin(), secondPacket.payload.end());
            }
            gb28181::ProgramStreamInfo packetizedPsInfo;
            rtpPacketizeOk =
                firstPacketOk &&
                secondPacketOk &&
                !firstPacket.marker &&
                secondPacket.marker &&
                firstPacket.sequenceNumber == 7 &&
                secondPacket.sequenceNumber == 8 &&
                firstPacket.timestamp == 90000 &&
                secondPacket.timestamp == 90000 &&
                firstPacket.ssrc == 1234567890U &&
                secondPacket.ssrc == 1234567890U &&
                reassembledPayload == psFramePayload &&
                gb28181::parseProgramStreamPayload(reassembledPayload, &packetizedPsInfo) &&
                packetizedPsInfo.videoPayloads.size() == 1 &&
                packetizedPsInfo.videoPayloads.front() == videoFramePayload;
        }
        const bool rtpFirstOk = rtpPacketizeOk && node.receiveRtpPacket(mediaSessionId, outboundRtpPackets[0], &mediaError);
        gb28181::MediaSessionInfo receivingSessionAfterFirstPacket;
        const bool framePendingOk =
            node.mediaSessionInfo(mediaSessionId, &receivingSessionAfterFirstPacket) &&
            receivingSessionAfterFirstPacket.completedVideoFrames == 0 &&
            receivingSessionAfterFirstPacket.currentRtpPayloadPackets == 1 &&
            receivingSessionAfterFirstPacket.currentRtpPayloadBytes > 0 &&
            receivingSessionAfterFirstPacket.currentVideoFramePackets == 0 &&
            receivingSessionAfterFirstPacket.currentVideoFrameBytes == 0;
        const bool rtpSecondOk = rtpPacketizeOk && node.receiveRtpPacket(mediaSessionId, outboundRtpPackets[1], &mediaError);
        const bool rtpOk = rtpFirstOk && framePendingOk && rtpSecondOk;
        gb28181::MediaSessionInfo receivingSession;
        const bool mediaReceivingOk =
            node.mediaSessionInfo(mediaSessionId, &receivingSession) &&
            receivingSession.state == "stream-receiving" &&
            receivingSession.receivedRtpPackets == 2 &&
            receivingSession.receivedPayloadBytes == psFramePayload.size() &&
            receivingSession.receivedPsPacks == 1 &&
            receivingSession.receivedPesPackets == 1 &&
            receivingSession.receivedVideoPesPackets == 1 &&
            receivingSession.receivedPsPayloadBytes == videoFramePayload.size() &&
            receivingSession.receivedVideoNalUnits == 2 &&
            receivingSession.receivedH264NalUnits == 2 &&
            receivingSession.receivedH264Idr == 1 &&
            receivingSession.receivedH265NalUnits == 0 &&
            receivingSession.completedVideoFrames == 1 &&
            receivingSession.incompleteVideoFrames == 0 &&
            receivingSession.currentVideoFrameBytes == 0 &&
            receivingSession.currentVideoFramePackets == 0 &&
            receivingSession.lastVideoFrameCodec == gb28181::VIDEO_CODEC_H264 &&
            receivingSession.lastVideoFrameKey &&
            receivingSession.lastVideoFrameBytes == videoFramePayload.size() &&
            receivingSession.lastVideoFramePackets == 2 &&
            receivingSession.lastRtpSequence == 8 &&
            receivingSession.lastRtpTimestamp == 90000 &&
            receivingSession.lastRtpSsrc == 1234567890U &&
            receivingSession.outOfOrderRtpPackets == 0;
        std::vector<std::vector<unsigned char> > adapterPackets;
        const bool adapterPacketizeOk = node.buildOutboundRtpPackets(mediaSessionId,
                                                                     videoFramePayload,
                                                                     9,
                                                                     180000,
                                                                     20,
                                                                     &adapterPackets,
                                                                     &mediaError);
        std::vector<gb28181::RtpPayloadPacket> sentPayloadPackets;
        const bool rtpAdapterStartOk = node.startRtpSessionAdapter(
            mediaSessionId,
            std::unique_ptr<gb28181::RtpSessionAdapter>(new SelfTestRtpSessionAdapter(adapterPackets, &sentPayloadPackets)),
            &mediaError);
        gb28181::MediaSessionInfo adapterReceivingSession;
        const bool rtpAdapterOk =
            adapterPacketizeOk &&
            rtpAdapterStartOk &&
            node.mediaSessionInfo(mediaSessionId, &adapterReceivingSession) &&
            adapterReceivingSession.receivedRtpPackets == 4 &&
            adapterReceivingSession.receivedPsPacks == 2 &&
            adapterReceivingSession.completedVideoFrames == 2 &&
            adapterReceivingSession.lastVideoFramePackets == 2 &&
            adapterReceivingSession.lastRtpSequence == 10 &&
            adapterReceivingSession.lastRtpTimestamp == 180000 &&
            adapterReceivingSession.outOfOrderRtpPackets == 0;
        const bool adapterSendCallOk = node.sendAnnexBFrame(mediaSessionId,
                                                            videoFramePayload,
                                                            3600,
                                                            20,
                                                            &mediaError);
        gb28181::MediaSessionInfo adapterSendingSession;
        std::vector<unsigned char> sentReassembledPayload;
        for (std::vector<gb28181::RtpPayloadPacket>::const_iterator sentPacket = sentPayloadPackets.begin();
             sentPacket != sentPayloadPackets.end();
             ++sentPacket)
        {
            sentReassembledPayload.insert(sentReassembledPayload.end(), sentPacket->payload.begin(), sentPacket->payload.end());
        }
        gb28181::ProgramStreamInfo sentPsInfo;
        const bool adapterSendOk =
            adapterSendCallOk &&
            sentPayloadPackets.size() == 2 &&
            !sentPayloadPackets[0].marker &&
            sentPayloadPackets[0].payloadType == 96 &&
            sentPayloadPackets[0].timestampIncrement == 0 &&
            sentPayloadPackets[1].marker &&
            sentPayloadPackets[1].payloadType == 96 &&
            sentPayloadPackets[1].timestampIncrement == 3600 &&
            sentReassembledPayload == psFramePayload &&
            gb28181::parseProgramStreamPayload(sentReassembledPayload, &sentPsInfo) &&
            sentPsInfo.videoPayloads.size() == 1 &&
            sentPsInfo.videoPayloads.front() == videoFramePayload &&
            node.mediaSessionInfo(mediaSessionId, &adapterSendingSession) &&
            adapterSendingSession.sentRtpPayloadPackets == 2 &&
            adapterSendingSession.sentRtpPayloadBytes == psFramePayload.size() &&
            adapterSendingSession.sentVideoFrames == 1;
        node.stopRtpSessionAdapter(mediaSessionId);
        std::ifstream frameFile(kSelfTestFramePath, std::ios::in | std::ios::binary);
        const std::vector<unsigned char> frameFileBytes((std::istreambuf_iterator<char>(frameFile)),
                                                        std::istreambuf_iterator<char>());
        std::vector<unsigned char> expectedFrameBytes;
        expectedFrameBytes.insert(expectedFrameBytes.end(), videoFramePayload.begin(), videoFramePayload.end());
        expectedFrameBytes.insert(expectedFrameBytes.end(), videoFramePayload.begin(), videoFramePayload.end());
        const bool frameFileOk = frameFileBytes == expectedFrameBytes;
        std::remove(kSelfTestFramePath);
        const bool wrongDialogByeRejected = !node.dispatchSipRequest(wrongDialogBye);
        const bool byeOk = node.dispatchSipRequest(bye);
        const bool md5Ok = gb28181::md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72";
        const size_t sentMessages = node.sentSipMessageCount();
        const size_t scheduledTasks = node.scheduledTaskCount();
        const size_t mediaSessions = node.mediaSessionCount();
        const size_t availableRtpPorts = node.availableRtpPortCount();
        const size_t catalogItems = node.catalogItemCount();
        const size_t recordItems = node.recordItemCount();
        const std::vector<gb28181::ManscdpItem> catalogSnapshot = node.catalogItems(kSelfTestDownstreamId);
        const std::vector<gb28181::ManscdpItem> recordSnapshot = node.recordItems(kSelfTestDownstreamId);
        const bool catalogSnapshotOk = catalogSnapshot.size() == 2 &&
                                       catalogSnapshot.front().deviceId == kSelfTestDownstreamId &&
                                       catalogSnapshot.front().name == "SelfTestCamera" &&
                                       catalogSnapshot.front().manufacturer == "media-fabric" &&
                                       catalogSnapshot.front().model == "SelfTestModel" &&
                                       catalogSnapshot.front().owner == "SelfTestOwner" &&
                                       catalogSnapshot.front().civilCode == "110000" &&
                                       catalogSnapshot.front().parental == "1" &&
                                       catalogSnapshot.front().parentId == "10000000002000000001" &&
                                       catalogSnapshot.front().safetyWay == "0" &&
                                       catalogSnapshot.front().registerWay == "1" &&
                                       catalogSnapshot.front().secrecy == "0" &&
                                       catalogSnapshot.front().status == "ON" &&
                                       catalogSnapshot[1].deviceId == "12000000001310000059" &&
                                       catalogSnapshot[1].name == "SelfTestCameraTwo";
        const bool recordSnapshotOk = recordSnapshot.size() == 1 &&
                                      recordSnapshot.front().deviceId == kSelfTestDownstreamId &&
                                      recordSnapshot.front().filePath == "/tmp/self-test.ps";
        const std::string businessStateSnapshot = node.businessStateSnapshot();
        std::string businessStateError;
        gb28181::GB28181Node restoredNode(config);
        const bool businessStateRestoreOk = restoredNode.restoreBusinessStateSnapshot(businessStateSnapshot, &businessStateError);
        const std::vector<gb28181::ManscdpItem> restoredCatalog = restoredNode.catalogItems(kSelfTestDownstreamId);
        const std::vector<gb28181::ManscdpItem> restoredRecords = restoredNode.recordItems(kSelfTestDownstreamId);
        const bool businessStateSaveOk = node.saveBusinessStateSnapshot(kSelfTestBusinessStatePath, &businessStateError);
        gb28181::GB28181Node loadedNode(config);
        const bool businessStateLoadOk = businessStateSaveOk &&
                                         loadedNode.loadBusinessStateSnapshot(kSelfTestBusinessStatePath, &businessStateError);
        std::remove(kSelfTestBusinessStatePath);
        const std::vector<gb28181::ManscdpItem> loadedCatalog = loadedNode.catalogItems(kSelfTestDownstreamId);
        const std::vector<gb28181::ManscdpItem> loadedRecords = loadedNode.recordItems(kSelfTestDownstreamId);
        const std::string businessSummaryJson = node.businessSummaryJson();
        const std::string catalogJson = node.catalogJson(kSelfTestDownstreamId);
        const std::string recordJson = node.recordJson(kSelfTestDownstreamId);
        std::string businessCliSummaryJson;
        std::string businessCliCatalogJson;
        std::string businessCliRecordJson;
        std::string businessCliError;
        const bool businessCliSummaryOk = queryBusinessJson(node,
                                                            "summary",
                                                            "",
                                                            &businessCliSummaryJson,
                                                            &businessCliError);
        const bool businessCliCatalogOk = queryBusinessJson(node,
                                                            "catalog",
                                                            kSelfTestDownstreamId,
                                                            &businessCliCatalogJson,
                                                            &businessCliError);
        const bool businessCliRecordOk = queryBusinessJson(node,
                                                           "record",
                                                           kSelfTestDownstreamId,
                                                           &businessCliRecordJson,
                                                           &businessCliError);
        const bool businessCliMissingPeerRejected = !queryBusinessJson(node,
                                                                       "catalog",
                                                                       "",
                                                                       &businessCliCatalogJson,
                                                                       &businessCliError);
        const bool businessQueryJsonOk =
            businessSummaryJson.find("\"catalog_items\":2") != std::string::npos &&
            businessSummaryJson.find("\"record_items\":1") != std::string::npos &&
            catalogJson.find("\"type\":\"catalog\"") != std::string::npos &&
            catalogJson.find("\"name\":\"SelfTestCamera\"") != std::string::npos &&
            catalogJson.find("\"parental\":\"1\"") != std::string::npos &&
            recordJson.find("\"type\":\"record\"") != std::string::npos &&
            recordJson.find("\"file_path\":\"/tmp/self-test.ps\"") != std::string::npos &&
            businessCliSummaryOk &&
            businessCliCatalogOk &&
            businessCliRecordOk &&
            businessCliMissingPeerRejected &&
            businessCliSummaryJson == businessSummaryJson &&
            businessCliCatalogJson == catalogJson &&
            businessCliRecordJson == recordJson;
        const bool businessStatePersistenceOk =
            businessStateRestoreOk &&
            businessStateLoadOk &&
            restoredCatalog.size() == 2 &&
            restoredRecords.size() == 1 &&
            loadedCatalog.size() == 2 &&
            loadedRecords.size() == 1 &&
            restoredCatalog.front().name == "SelfTestCamera" &&
            restoredCatalog.front().parental == "1" &&
            restoredRecords.front().filePath == "/tmp/self-test.ps" &&
            loadedCatalog.front().deviceId == kSelfTestDownstreamId &&
            loadedCatalog.front().parental == "1" &&
            loadedCatalog[1].deviceId == "12000000001310000059" &&
            loadedRecords.front().filePath == "/tmp/self-test.ps";
        const bool rtpPortsReleased = availableRtpPorts == initialRtpPorts;
        std::cout << "self-test dispatch REGISTER_CHALLENGE=" << (regChallengeOk ? "ok" : "failed")
                  << " MEDIA_SOURCE=" << (mediaSourceOk ? "ok" : "failed")
                  << " MEDIA_SOURCE_PS=" << (mediaSourcePsOk ? "ok" : "failed")
                  << " CHALLENGE_NONCE=" << (challengeNonceOk ? "ok" : "failed")
                  << " REGISTER_AUTH_RETRY=" << (registerAuthRetryOk ? "ok" : "failed")
                  << " BAD_REGISTER=" << (badRegRejected ? "rejected" : "accepted")
                  << " REGISTER=" << (regOk ? "ok" : "failed")
                  << " REPLAY_REGISTER=" << (replayRegRejected ? "rejected" : "accepted")
                  << " QOP_CHALLENGE=" << (qopChallengeOk && qopChallengeNonceOk ? "ok" : "failed")
                  << " QOP_REGISTER=" << (qopRegOk ? "ok" : "failed")
                  << " QOP_REPLAY=" << (qopReplayRejected ? "rejected" : "accepted")
                  << " BAD_KEEPALIVE_XML=" << (badKeepaliveRejected ? "rejected" : "accepted")
                  << " KEEPALIVE=" << (keepaliveOk ? "ok" : "failed")
                  << " CATALOG=" << (catalogOk ? "ok" : "failed")
                  << " RECORD=" << (recordOk ? "ok" : "failed")
                  << " BAD_CATALOG_RESPONSE=" << (badCatalogResponseRejected ? "rejected" : "accepted")
                  << " CATALOG_RESPONSE=" << (catalogResponseOk ? "ok" : "failed")
                  << " CATALOG_ROUTE=" << (catalogRouteResolved && catalogRouteAmbiguous && catalogRouteMissing ? "ok" : "failed")
                  << " BAD_RECORD_RESPONSE=" << (badRecordResponseRejected ? "rejected" : "accepted")
                  << " RECORD_RESPONSE=" << (recordResponseOk ? "ok" : "failed")
                  << " INVITE_RESPONSE=" << (inviteResponseOk ? "ok" : "failed")
                  << " INVITE_ACK=" << (inviteAckOk ? "ok" : "failed")
                  << " BAD_INVITE=" << (badInviteRejected ? "rejected" : "accepted")
                  << " EARLY_ACK=" << (earlyAckRejected ? "rejected" : "accepted")
                  << " INVITE=" << (inviteOk ? "ok" : "failed")
                  << " BAD_ACK_CSEQ=" << (badAckCseqRejected ? "rejected" : "accepted")
                  << " ACK=" << (ackOk ? "ok" : "failed")
                  << " BAD_RTP_SSRC=" << (badRtpSsrcRejected ? "rejected" : "accepted")
                  << " RTP_PACKETIZE=" << (rtpPacketizeOk ? "ok" : "failed")
                  << " RTP=" << (rtpOk ? "ok" : "failed")
                  << " PS=" << (receivingSession.receivedVideoPesPackets == 1 ? "ok" : "failed")
                  << " H264=" << (receivingSession.receivedH264Idr == 1 ? "ok" : "failed")
                  << " FRAME=" << (receivingSession.completedVideoFrames == 1 && receivingSession.lastVideoFramePackets == 2 ? "ok" : "failed")
                  << " FRAME_FILE=" << (frameFileOk ? "ok" : "failed")
                  << " RTP_ADAPTER=" << (rtpAdapterOk ? "ok" : "failed")
                  << " SEND_ADAPTER=" << (adapterSendOk ? "ok" : "failed")
                  << " MEDIA_RECEIVING=" << (mediaReceivingOk ? "ok" : "failed")
                  << " WRONG_DIALOG_BYE=" << (wrongDialogByeRejected ? "rejected" : "accepted")
                  << " BYE=" << (byeOk ? "ok" : "failed")
                  << " MD5=" << (md5Ok ? "ok" : "failed")
                  << " sessions=" << node.sessionCount()
                  << " endpoints=" << node.endpointCount()
                  << " upstream_peers=" << node.upstreamPeerCount()
                  << " downstream_peers=" << node.downstreamPeerCount()
                  << " registered_peers=" << node.registeredPeerCount()
                  << " keepalive_peers=" << node.keepalivePeerCount()
                  << " routes=" << node.routeCount()
                  << " sent_messages=" << sentMessages
                  << " scheduled_tasks=" << scheduledTasks
                  << " catalog_items=" << catalogItems
                  << " record_items=" << recordItems
                  << " catalog_snapshot=" << (catalogSnapshotOk ? "ok" : "failed")
                  << " record_snapshot=" << (recordSnapshotOk ? "ok" : "failed")
                  << " business_state_restore=" << (businessStateRestoreOk ? "ok" : "failed")
                  << " business_state_file=" << (businessStateLoadOk ? "ok" : "failed")
                  << " business_query_json=" << (businessQueryJsonOk ? "ok" : "failed")
                  << " business_query_cli=" << (businessQueryJsonOk ? "ok" : "failed")
                  << " media_sessions=" << mediaSessions
                  << " available_rtp_ports=" << availableRtpPorts
                  << std::endl;

        if (!singleEndpointOk || !peerTopologyOk || !mediaSourceOk || !mediaSourcePsOk || !regChallengeOk || !challengeNonceOk || !registerAuthRetryOk || !badRegRejected || !regOk || !replayRegRejected || !qopChallengeOk || !qopChallengeNonceOk || !qopRegOk || !qopReplayRejected || !badKeepaliveRejected || !keepaliveOk || !catalogOk || !recordOk || !badCatalogResponseRejected || !catalogResponseOk || !catalogResponseSecondOk || !catalogRouteResolved || !catalogRouteAmbiguous || !catalogRouteMissing || !badRecordResponseRejected || !recordResponseOk || !inviteResponseOk || !inviteAckOk || catalogItems != 2 || recordItems != 1 || !catalogSnapshotOk || !recordSnapshotOk || !businessStatePersistenceOk || !businessQueryJsonOk || !badInviteRejected || !earlyAckRejected || !inviteOk || !badAckCseqRejected || !ackOk || !badRtpSsrcRejected || !rtpPacketizeOk || !rtpOk || !mediaReceivingOk || !rtpAdapterOk || !adapterSendOk || !frameFileOk || !wrongDialogByeRejected || !byeOk || !md5Ok || sentMessages == 0 || scheduledTasks < 3 || mediaSessions != 0 || !rtpPortsReleased)
        {
            node.stop();
            return 1;
        }
    }
    else
    {
        waitForServerStopSignal();
    }
    node.stop();
    managementServer.stop();
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "server", "stopped");

    return 0;
}
