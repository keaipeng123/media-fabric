#include "GB28181Node.h"
#include "DigestAuth.h"
#include "ManscdpMessage.h"
#include "MediaFrameSink.h"
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
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* kDefaultConfigPath = "conf/gb28181-server.conf";
const char* kSelfTestServerId = "10000000002000000001";
const char* kSelfTestClientId = "11000000002000000001";
const char* kSelfTestServerIp = "127.0.1";
const int kSelfTestServerPort = 5061;
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
    std::cout << "Usage: " << programName << " [-c config_path] [--self-test]" << std::endl;
}

std::string selfTestRegisterUri()
{
    return std::string("sip:") + kSelfTestServerId + "@" + kSelfTestServerIp + ":" + std::to_string(kSelfTestServerPort);
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
    request->digestAuth.uri = selfTestRegisterUri();
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
                "<Manufacturer>GB28181-Server</Manufacturer>\r\n"
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

} // namespace

int main(int argc, char* argv[])
{
    std::string configPath = kDefaultConfigPath;
    bool selfTest = false;

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

    gb28181::GB28181Node node(config);
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

    std::cout << "gb28181-server starting" << std::endl;
    if (!node.start())
    {
        return 1;
    }

    std::cout << "gb28181-server started" << std::endl;
    if (selfTest)
    {
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
        regChallenge.fromId = kSelfTestClientId;
        regChallenge.toId = kSelfTestServerId;
        regChallenge.expires = 60;

        gb28181::SipRequestContext invite;
        invite.method = "INVITE";
        invite.event = "request";
        invite.fromId = kSelfTestServerId;
        invite.toId = kSelfTestClientId;
        invite.callId = "selftest-invite-dialog";
        invite.cseq = "1";
        invite.contact = "<sip:10000000002000000001@127.0.1:5061>";
        invite.body = gb28181::buildInviteSdp(kSelfTestClientId, kSelfTestServerIp, 30000, "1234567890");

        gb28181::SipRequestContext badInvite = invite;
        badInvite.body = "v=0\r\ns=Bad\r\n";

        gb28181::SipRequestContext ack;
        ack.method = "ACK";
        ack.event = "request";
        ack.fromId = kSelfTestServerId;
        ack.toId = kSelfTestClientId;
        ack.callId = invite.callId;
        ack.cseq = invite.cseq;

        gb28181::SipRequestContext badAckCseq = ack;
        badAckCseq.cseq = "2";

        gb28181::SipRequestContext keepalive;
        keepalive.method = "MESSAGE";
        keepalive.fromId = kSelfTestClientId;
        keepalive.toId = kSelfTestServerId;
        fillManscdp(&keepalive, makeSelfTestKeepaliveXml(kSelfTestClientId));

        gb28181::SipRequestContext badKeepalive = keepalive;
        fillManscdp(&badKeepalive, makeSelfTestKeepaliveXml("00000000000000000000"));

        gb28181::SipRequestContext catalog;
        catalog.method = "MESSAGE";
        catalog.fromId = kSelfTestServerId;
        catalog.toId = kSelfTestClientId;
        fillManscdp(&catalog, makeSelfTestQueryXml("Catalog", kSelfTestClientId));

        gb28181::SipRequestContext record;
        record.method = "MESSAGE";
        record.fromId = kSelfTestServerId;
        record.toId = kSelfTestClientId;
        fillManscdp(&record, makeSelfTestQueryXml("RecordInfo", kSelfTestClientId));

        gb28181::SipRequestContext catalogResponse;
        catalogResponse.method = "MESSAGE";
        catalogResponse.fromId = kSelfTestClientId;
        catalogResponse.toId = kSelfTestServerId;
        fillManscdp(&catalogResponse, makeSelfTestResponseXml("Catalog", kSelfTestClientId));

        gb28181::SipRequestContext badCatalogResponse = catalogResponse;
        fillManscdp(&badCatalogResponse, makeSelfTestBadResponseXml("Catalog", kSelfTestClientId));

        gb28181::SipRequestContext recordResponse;
        recordResponse.method = "MESSAGE";
        recordResponse.fromId = kSelfTestClientId;
        recordResponse.toId = kSelfTestServerId;
        fillManscdp(&recordResponse, makeSelfTestResponseXml("RecordInfo", kSelfTestClientId));

        gb28181::SipRequestContext badRecordResponse = recordResponse;
        fillManscdp(&badRecordResponse, makeSelfTestBadResponseXml("RecordInfo", kSelfTestClientId));

        gb28181::SipRequestContext bye;
        bye.method = "BYE";
        bye.event = "request";
        bye.fromId = kSelfTestServerId;
        bye.toId = kSelfTestClientId;
        bye.callId = invite.callId;
        bye.cseq = "3";

        gb28181::SipRequestContext wrongDialogBye = bye;
        wrongDialogBye.callId = "missing-dialog";

        const bool regChallengeOk = node.dispatchSipRequest(regChallenge);
        gb28181::SipMessageContext challengeMessage;
        const bool challengeCaptured = node.lastSentSipMessage(&challengeMessage);
        const std::string challengeNonce = challengeMessage.authNonce;
        const bool challengeNonceOk = challengeCaptured &&
                                      challengeMessage.response &&
                                      challengeMessage.statusCode == 401 &&
                                      !challengeNonce.empty();

        gb28181::SipRequestContext reg;
        reg.method = "REGISTER";
        reg.event = "request";
        reg.fromId = kSelfTestClientId;
        reg.toId = kSelfTestServerId;
        reg.expires = 60;
        fillRegisterDigest(&reg,
                           challengeNonce,
                           gb28181::computeDigestResponse(reg.method,
                                                          kSelfTestUsername,
                                                          kSelfTestRealm,
                                                          kSelfTestPassword,
                                                          challengeNonce,
                                                          selfTestRegisterUri(),
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
                                                          selfTestRegisterUri(),
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
        const bool badRecordResponseRejected = !node.dispatchSipRequest(badRecordResponse);
        const bool recordResponseOk = node.dispatchSipRequest(recordResponse);
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
        const std::vector<gb28181::ManscdpItem> catalogSnapshot = node.catalogItems(kSelfTestClientId);
        const std::vector<gb28181::ManscdpItem> recordSnapshot = node.recordItems(kSelfTestClientId);
        const bool catalogSnapshotOk = catalogSnapshot.size() == 1 &&
                                       catalogSnapshot.front().deviceId == kSelfTestClientId &&
                                       catalogSnapshot.front().name == "SelfTestCamera";
        const bool recordSnapshotOk = recordSnapshot.size() == 1 &&
                                      recordSnapshot.front().deviceId == kSelfTestClientId &&
                                      recordSnapshot.front().filePath == "/tmp/self-test.ps";
        const std::string businessStateSnapshot = node.businessStateSnapshot();
        std::string businessStateError;
        gb28181::GB28181Node restoredNode(config);
        const bool businessStateRestoreOk = restoredNode.restoreBusinessStateSnapshot(businessStateSnapshot, &businessStateError);
        const std::vector<gb28181::ManscdpItem> restoredCatalog = restoredNode.catalogItems(kSelfTestClientId);
        const std::vector<gb28181::ManscdpItem> restoredRecords = restoredNode.recordItems(kSelfTestClientId);
        const bool businessStateSaveOk = node.saveBusinessStateSnapshot(kSelfTestBusinessStatePath, &businessStateError);
        gb28181::GB28181Node loadedNode(config);
        const bool businessStateLoadOk = businessStateSaveOk &&
                                         loadedNode.loadBusinessStateSnapshot(kSelfTestBusinessStatePath, &businessStateError);
        std::remove(kSelfTestBusinessStatePath);
        const std::vector<gb28181::ManscdpItem> loadedCatalog = loadedNode.catalogItems(kSelfTestClientId);
        const std::vector<gb28181::ManscdpItem> loadedRecords = loadedNode.recordItems(kSelfTestClientId);
        const std::string businessSummaryJson = node.businessSummaryJson();
        const std::string catalogJson = node.catalogJson(kSelfTestClientId);
        const std::string recordJson = node.recordJson(kSelfTestClientId);
        const bool businessQueryJsonOk =
            businessSummaryJson.find("\"catalog_items\":1") != std::string::npos &&
            businessSummaryJson.find("\"record_items\":1") != std::string::npos &&
            catalogJson.find("\"type\":\"catalog\"") != std::string::npos &&
            catalogJson.find("\"name\":\"SelfTestCamera\"") != std::string::npos &&
            recordJson.find("\"type\":\"record\"") != std::string::npos &&
            recordJson.find("\"file_path\":\"/tmp/self-test.ps\"") != std::string::npos;
        const bool businessStatePersistenceOk =
            businessStateRestoreOk &&
            businessStateLoadOk &&
            restoredCatalog.size() == 1 &&
            restoredRecords.size() == 1 &&
            loadedCatalog.size() == 1 &&
            loadedRecords.size() == 1 &&
            restoredCatalog.front().name == "SelfTestCamera" &&
            restoredRecords.front().filePath == "/tmp/self-test.ps" &&
            loadedCatalog.front().deviceId == kSelfTestClientId &&
            loadedRecords.front().filePath == "/tmp/self-test.ps";
        const bool rtpPortsReleased = availableRtpPorts == initialRtpPorts;
        std::cout << "self-test dispatch REGISTER_CHALLENGE=" << (regChallengeOk ? "ok" : "failed")
                  << " MEDIA_SOURCE=" << (mediaSourceOk ? "ok" : "failed")
                  << " MEDIA_SOURCE_PS=" << (mediaSourcePsOk ? "ok" : "failed")
                  << " CHALLENGE_NONCE=" << (challengeNonceOk ? "ok" : "failed")
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
                  << " BAD_RECORD_RESPONSE=" << (badRecordResponseRejected ? "rejected" : "accepted")
                  << " RECORD_RESPONSE=" << (recordResponseOk ? "ok" : "failed")
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
                  << " media_sessions=" << mediaSessions
                  << " available_rtp_ports=" << availableRtpPorts
                  << std::endl;

        if (!mediaSourceOk || !mediaSourcePsOk || !regChallengeOk || !challengeNonceOk || !badRegRejected || !regOk || !replayRegRejected || !qopChallengeOk || !qopChallengeNonceOk || !qopRegOk || !qopReplayRejected || !badKeepaliveRejected || !keepaliveOk || !catalogOk || !recordOk || !badCatalogResponseRejected || !catalogResponseOk || !badRecordResponseRejected || !recordResponseOk || catalogItems != 1 || recordItems != 1 || !catalogSnapshotOk || !recordSnapshotOk || !businessStatePersistenceOk || !businessQueryJsonOk || !badInviteRejected || !earlyAckRejected || !inviteOk || !badAckCseqRejected || !ackOk || !badRtpSsrcRejected || !rtpPacketizeOk || !rtpOk || !mediaReceivingOk || !rtpAdapterOk || !adapterSendOk || !frameFileOk || !wrongDialogByeRejected || !byeOk || !md5Ok || sentMessages == 0 || scheduledTasks < 3 || mediaSessions != 0 || !rtpPortsReleased)
        {
            node.stop();
            return 1;
        }
    }
    node.stop();
    std::cout << "gb28181-server stopped" << std::endl;

    return 0;
}
