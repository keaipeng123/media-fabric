#ifndef GB28181_CORE_SIPTRANSPORT_H
#define GB28181_CORE_SIPTRANSPORT_H

#include "SipEndpoint.h"
#include "SipMessageContext.h"
#include "SipRequestContext.h"

#include <functional>
#include <vector>

namespace gb28181 {

class SipTransport
{
public:
    typedef std::function<bool(const SipRequestContext&)> RequestHandler;

    virtual ~SipTransport() {}

    virtual bool start(const std::vector<SipListenEndpoint>& endpoints, const RequestHandler& handler) = 0;
    virtual void stop() = 0;
    virtual bool send(const SipMessageContext& message) = 0;
    virtual bool running() const = 0;
    virtual size_t sentMessageCount() const = 0;
};

class InMemorySipTransport : public SipTransport
{
public:
    InMemorySipTransport();

    bool start(const std::vector<SipListenEndpoint>& endpoints, const RequestHandler& handler);
    void stop();
    bool send(const SipMessageContext& message);
    bool running() const;
    size_t sentMessageCount() const;
    const std::vector<SipMessageContext>& sentMessages() const;

private:
    bool m_running;
    std::vector<SipMessageContext> m_sentMessages;
};

} // namespace gb28181

#endif
