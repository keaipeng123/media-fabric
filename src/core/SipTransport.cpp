#include "SipTransport.h"

#include <iostream>

namespace gb28181 {

InMemorySipTransport::InMemorySipTransport()
    : m_running(false)
{
}

bool InMemorySipTransport::start(const std::vector<SipListenEndpoint>& endpoints, const RequestHandler& handler)
{
    if (endpoints.empty() || !handler)
    {
        return false;
    }

    m_running = true;
    return true;
}

void InMemorySipTransport::stop()
{
    m_running = false;
}

bool InMemorySipTransport::send(const SipMessageContext& message)
{
    if (!m_running || message.method.empty())
    {
        return false;
    }

    m_sentMessages.push_back(message);

    std::cout << "  sip send: ";
    if (message.response)
    {
        std::cout << "response " << message.statusCode << " ";
    }
    else
    {
        std::cout << "request ";
    }
    std::cout << message.method;
    if (!message.event.empty())
    {
        std::cout << "/" << message.event;
    }
    std::cout << " " << message.fromId << " -> " << message.toId;
    if (!message.remoteIp.empty() && message.remotePort > 0)
    {
        std::cout << " @" << message.remoteIp << ":" << message.remotePort;
    }
    if (!message.authRealm.empty())
    {
        std::cout << " auth-realm=" << message.authRealm;
    }
    if (!message.body.empty())
    {
        std::cout << " body-bytes=" << message.body.size();
    }
    std::cout << std::endl;
    return true;
}

bool InMemorySipTransport::running() const
{
    return m_running;
}

size_t InMemorySipTransport::sentMessageCount() const
{
    return m_sentMessages.size();
}

const std::vector<SipMessageContext>& InMemorySipTransport::sentMessages() const
{
    return m_sentMessages;
}

} // namespace gb28181
