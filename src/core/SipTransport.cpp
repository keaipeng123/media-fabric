#include "SipTransport.h"

#include "Logger.h"

#include <sstream>

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

    std::ostringstream detail;
    detail << "direction=outbound ";
    if (message.response)
    {
        detail << "response status=" << message.statusCode << " ";
    }
    else
    {
        detail << "request ";
    }
    detail << "method=" << message.method;
    if (!message.event.empty())
    {
        detail << " event=" << message.event;
    }
    detail << " from=" << message.fromId << " to=" << message.toId;
    if (!message.remoteIp.empty() && message.remotePort > 0)
    {
        detail << " remote=" << message.remoteIp << ":" << message.remotePort;
    }
    if (!message.callId.empty())
    {
        detail << " call_id=" << message.callId;
    }
    if (!message.cseq.empty())
    {
        detail << " cseq=" << message.cseq;
    }
    if (!message.authRealm.empty())
    {
        detail << " auth_realm=" << message.authRealm;
    }
    if (!message.digestAuth.response.empty())
    {
        detail << " authorization_user=" << message.digestAuth.username;
    }
    if (!message.body.empty())
    {
        detail << " body_bytes=" << message.body.size();
    }
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "sip", detail.str());
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
