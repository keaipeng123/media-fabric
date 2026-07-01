#include "JrtplibRtpSessionAdapter.h"

#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpsourcedata.h>
#include <jrtplib3/rtptransmitter.h>
#include <jrtplib3/rtpudpv4transmitter.h>

#include <arpa/inet.h>

namespace gb28181 {

JrtplibRtpSessionAdapter::JrtplibRtpSessionAdapter()
    : m_options(),
      m_callback(),
      m_running(false)
{
}

JrtplibRtpSessionAdapter::~JrtplibRtpSessionAdapter()
{
    stop();
}

bool JrtplibRtpSessionAdapter::start(const RtpSessionOptions& options,
                                     RtpPacketCallback callback,
                                     std::string* error)
{
    if (m_running)
    {
        setError(error, "JRTPLIB RTP session already running");
        return false;
    }
    if (options.sessionId.empty())
    {
        setError(error, "empty RTP media session id");
        return false;
    }
    if (options.localRtpPort <= 0)
    {
        setError(error, "invalid RTP local port");
        return false;
    }
    if (!callback)
    {
        setError(error, "empty RTP packet callback");
        return false;
    }

    jrtplib::RTPSessionParams sessionParams;
    sessionParams.SetOwnTimestampUnit(1.0 / 90000.0);
    sessionParams.SetAcceptOwnPackets(false);
    sessionParams.SetUsePollThread(options.usePollThread);
    sessionParams.SetNeedThreadSafety(true);
    sessionParams.SetMinimumRTCPTransmissionInterval(jrtplib::RTPTime(5, 0));

    jrtplib::RTPUDPv4TransmissionParams transportParams;
    transportParams.SetPortbase(static_cast<uint16_t>(options.localRtpPort));
    transportParams.SetRTPReceiveBuffer(options.receiveBufferBytes);
    transportParams.SetRTCPReceiveBuffer(options.receiveBufferBytes);
    transportParams.SetRTPSendBuffer(options.sendBufferBytes);
    transportParams.SetMulticastTTL(options.multicastTtl);

    const int createResult = Create(sessionParams, &transportParams, jrtplib::RTPTransmitter::IPv4UDPProto);
    if (createResult < 0)
    {
        setError(error, jrtplib::RTPGetErrorString(createResult));
        return false;
    }

    const int maxPacketResult = SetMaximumPacketSize(options.maximumPacketSize);
    if (maxPacketResult < 0)
    {
        Destroy();
        setError(error, jrtplib::RTPGetErrorString(maxPacketResult));
        return false;
    }

    if (!options.remoteIp.empty() && options.remoteRtpPort > 0)
    {
        const in_addr_t remoteAddress = inet_addr(options.remoteIp.c_str());
        if (remoteAddress == INADDR_NONE)
        {
            Destroy();
            setError(error, "invalid RTP remote IP: " + options.remoteIp);
            return false;
        }

        jrtplib::RTPIPv4Address destination(ntohl(remoteAddress),
                                            static_cast<uint16_t>(options.remoteRtpPort));
        const int destinationResult = AddDestination(destination);
        if (destinationResult < 0)
        {
            Destroy();
            setError(error, jrtplib::RTPGetErrorString(destinationResult));
            return false;
        }
    }

    m_options = options;
    m_callback = callback;
    m_running = true;
    return true;
}

bool JrtplibRtpSessionAdapter::sendPayloadPacket(const RtpPayloadPacket& packet, std::string* error)
{
    if (!m_running)
    {
        setError(error, "JRTPLIB RTP session is not running");
        return false;
    }
    if (packet.payload.empty())
    {
        setError(error, "empty RTP payload packet");
        return false;
    }

    const int result = SendPacket(&packet.payload[0],
                                  packet.payload.size(),
                                  packet.payloadType,
                                  packet.marker,
                                  packet.timestampIncrement);
    if (result < 0)
    {
        setError(error, jrtplib::RTPGetErrorString(result));
        return false;
    }
    return true;
}

void JrtplibRtpSessionAdapter::stop()
{
    if (!m_running)
    {
        return;
    }

    BYEDestroy(jrtplib::RTPTime(0, 0), NULL, 0);
    m_callback = RtpPacketCallback();
    m_running = false;
}

bool JrtplibRtpSessionAdapter::running() const
{
    return m_running;
}

void JrtplibRtpSessionAdapter::OnRTPPacket(jrtplib::RTPPacket* packet,
                                           const jrtplib::RTPTime& receiveTime,
                                           const jrtplib::RTPAddress* senderAddress)
{
    (void)receiveTime;
    (void)senderAddress;
    if (!packet || !m_callback)
    {
        return;
    }

    const unsigned char* data = packet->GetPacketData();
    const size_t bytes = packet->GetPacketLength();
    if (data == NULL || bytes == 0)
    {
        return;
    }

    m_callback(m_options.sessionId, std::vector<unsigned char>(data, data + bytes));
}

void JrtplibRtpSessionAdapter::OnBYEPacket(jrtplib::RTPSourceData* sourceData)
{
    (void)sourceData;
}

void JrtplibRtpSessionAdapter::setError(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

} // namespace gb28181
