#ifndef GB28181_CORE_SIPSTACK_H
#define GB28181_CORE_SIPSTACK_H

#include "NodeConfig.h"
#include "SipEndpoint.h"
#include "SipMessageContext.h"
#include "SipRequestContext.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gb28181 {

class SipTransport;

struct SipRoute
{
    std::string method;
    std::string event;
    std::string capability;
    std::function<bool(const SipRequestContext&)> handler;
};

class SipStack
{
public:
    SipStack();
    ~SipStack();

    bool configure(const NodeConfig& config);
    bool start();
    void stop();

    bool registerHandler(const std::string& method,
                         const std::string& event,
                         const std::string& capabilityName,
                         const std::function<bool(const SipRequestContext&)>& handler);
    void unregisterHandlers(const std::string& capabilityName);
    bool dispatch(const SipRequestContext& request) const;
    bool send(const SipMessageContext& message);

    bool started() const;
    size_t endpointCount() const;
    size_t routeCount() const;
    size_t sentMessageCount() const;
    bool lastSentMessage(SipMessageContext* message) const;
    const std::vector<SipListenEndpoint>& endpoints() const;
    const std::vector<SipRoute>& routes() const;

private:
    std::vector<SipListenEndpoint> m_endpoints;
    std::vector<SipRoute> m_routes;
    std::unique_ptr<SipTransport> m_transport;
    bool m_started;
};

} // namespace gb28181

#endif
