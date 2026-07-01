#include "RtpSessionAdapter.h"

namespace gb28181 {

RtpPayloadPacket::RtpPayloadPacket()
    : payload(),
      payloadType(96),
      marker(false),
      timestampIncrement(0)
{
}

RtpSessionOptions::RtpSessionOptions()
    : localRtpPort(0),
      remoteIp(),
      remoteRtpPort(0),
      maximumPacketSize(1500),
      receiveBufferBytes(1024 * 1024),
      sendBufferBytes(30 * 1024),
      multicastTtl(255),
      usePollThread(true)
{
}

RtpSessionAdapter::~RtpSessionAdapter()
{
}

} // namespace gb28181
