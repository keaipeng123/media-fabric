#include "SipStack.h"

#include "SipTransport.h"
#ifdef GB28181_ENABLE_PJSIP
#include "PjsipStackAdapter.h"
#endif

#include <iostream>

namespace gb28181 {

SipStack::SipStack()
    :
#ifdef GB28181_ENABLE_PJSIP
      m_transport(new PjsipStackAdapter()),
#else
      m_transport(new InMemorySipTransport()),
#endif
      m_started(false)
{
}

SipStack::~SipStack()
{
    stop();
}

bool SipStack::configure(const NodeConfig& config)
{
    m_endpoints.clear();
    const std::vector<SipEndpointConfig>& configEndpoints = config.sipEndpoints();
    for (std::vector<SipEndpointConfig>::const_iterator it = configEndpoints.begin();
         it != configEndpoints.end();
         ++it)
    {
        SipListenEndpoint endpoint;
        endpoint.name = it->name;
        endpoint.sipId = it->sipId;
        endpoint.ip = it->sipIp;
        endpoint.port = it->sipPort;
        m_endpoints.push_back(endpoint);
    }

    return !m_endpoints.empty();
}

bool SipStack::start()
{
    if (m_started)
    {
        return true;
    }

    for (std::vector<SipListenEndpoint>::const_iterator it = m_endpoints.begin();
         it != m_endpoints.end();
         ++it)
    {
        std::cout << "sip endpoint: " << it->name << " " << it->sipId << "@" << it->ip << ":" << it->port << std::endl;
    }

    if (!m_transport->start(m_endpoints, [this](const SipRequestContext& request) {
            return dispatch(request);
        }))
    {
        return false;
    }

    m_started = true;
    return true;
}

void SipStack::stop()
{
    m_routes.clear();
    if (m_transport)
    {
        m_transport->stop();
    }
    m_started = false;
}

bool SipStack::registerHandler(const std::string& method,
                               const std::string& event,
                               const std::string& capabilityName,
                               const std::function<bool(const SipRequestContext&)>& handler)
{
    if (method.empty() || capabilityName.empty() || !handler)
    {
        return false;
    }

    SipRoute route;
    route.method = method;
    route.event = event;
    route.capability = capabilityName;
    route.handler = handler;
    m_routes.push_back(route);

    std::cout << "  sip route: " << method;
    if (!event.empty())
    {
        std::cout << "/" << event;
    }
    std::cout << " -> " << capabilityName << std::endl;
    return true;
}

void SipStack::unregisterHandlers(const std::string& capabilityName)
{
    for (std::vector<SipRoute>::iterator it = m_routes.begin(); it != m_routes.end();)
    {
        if (it->capability == capabilityName)
        {
            it = m_routes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool SipStack::dispatch(const SipRequestContext& request) const
{
    for (std::vector<SipRoute>::const_iterator it = m_routes.begin(); it != m_routes.end(); ++it)
    {
        if (it->method != request.method)
        {
            continue;
        }
        if (!it->event.empty() && it->event != request.event)
        {
            continue;
        }
        return it->handler(request);
    }
    return false;
}

bool SipStack::send(const SipMessageContext& message)
{
    if (!m_started || !m_transport)
    {
        return false;
    }
    return m_transport->send(message);
}

bool SipStack::started() const
{
    return m_started;
}

size_t SipStack::endpointCount() const
{
    return m_endpoints.size();
}

size_t SipStack::routeCount() const
{
    return m_routes.size();
}

size_t SipStack::sentMessageCount() const
{
    return m_transport ? m_transport->sentMessageCount() : 0;
}

bool SipStack::lastSentMessage(SipMessageContext* message) const
{
    if (message == NULL || !m_transport)
    {
        return false;
    }

    const InMemorySipTransport* transport = dynamic_cast<const InMemorySipTransport*>(m_transport.get());
    if (transport == NULL || transport->sentMessages().empty())
    {
        return false;
    }

    *message = transport->sentMessages().back();
    return true;
}

const std::vector<SipListenEndpoint>& SipStack::endpoints() const
{
    return m_endpoints;
}

const std::vector<SipRoute>& SipStack::routes() const
{
    return m_routes;
}

} // namespace gb28181
