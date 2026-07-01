#ifndef GB28181_ADAPTERS_PJSIP_PJSIPSTACKADAPTER_H
#define GB28181_ADAPTERS_PJSIP_PJSIPSTACKADAPTER_H

#include "SipTransport.h"

#include <atomic>
#include <thread>

#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjlib.h>

namespace gb28181 {

class PjsipStackAdapter : public SipTransport
{
public:
    PjsipStackAdapter();
    ~PjsipStackAdapter();

    bool start(const std::vector<SipListenEndpoint>& endpoints, const RequestHandler& handler);
    void stop();
    bool send(const SipMessageContext& message);
    bool running() const;
    size_t sentMessageCount() const;
    bool handleRxRequest(pjsip_rx_data* rdata);

private:
    bool initPjlib();
    bool initEndpoint();
    bool startTransport(int sipPort);
    bool registerRecvModule();
    bool sendStatelessResponse(pjsip_rx_data* rdata, const SipMessageContext& response);
    void eventLoop();
    pjsip_endpoint* m_endpoint;
    pjmedia_endpt* m_mediaEndpoint;
    pj_caching_pool m_cachingPool;
    pj_pool_t* m_pool;
    RequestHandler m_handler;
    std::atomic<bool> m_running;
    size_t m_sentMessageCount;
    bool m_hasPendingResponse;
    SipMessageContext m_pendingResponse;
    bool m_pjlibInitialized;
    bool m_cachingPoolInitialized;
    std::thread m_eventThread;
};

} // namespace gb28181

#endif
