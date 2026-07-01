#ifndef GB28181_ADAPTERS_JRTPLIB_JRTPLIBRTPSESSIONADAPTER_H
#define GB28181_ADAPTERS_JRTPLIB_JRTPLIBRTPSESSIONADAPTER_H

#include "RtpSessionAdapter.h"

#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtptimeutilities.h>

#include <string>

namespace jrtplib {
class RTPAddress;
class RTPSourceData;
}

namespace gb28181 {

class JrtplibRtpSessionAdapter : public RtpSessionAdapter, private jrtplib::RTPSession
{
public:
    JrtplibRtpSessionAdapter();
    ~JrtplibRtpSessionAdapter();

    bool start(const RtpSessionOptions& options, RtpPacketCallback callback, std::string* error);
    bool sendPayloadPacket(const RtpPayloadPacket& packet, std::string* error);
    void stop();
    bool running() const;

private:
    void OnRTPPacket(jrtplib::RTPPacket* packet,
                     const jrtplib::RTPTime& receiveTime,
                     const jrtplib::RTPAddress* senderAddress);
    void OnBYEPacket(jrtplib::RTPSourceData* sourceData);
    static void setError(std::string* error, const std::string& message);

    RtpSessionOptions m_options;
    RtpPacketCallback m_callback;
    bool m_running;
};

} // namespace gb28181

#endif
