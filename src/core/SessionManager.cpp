#include "SessionManager.h"

namespace gb28181 {

bool SessionManager::createSession(const SessionInfo& session)
{
    if (session.id.empty())
    {
        return false;
    }

    m_sessions[session.id] = session;
    return true;
}

bool SessionManager::closeSession(const std::string& sessionId)
{
    return m_sessions.erase(sessionId) > 0;
}

size_t SessionManager::sessionCount() const
{
    return m_sessions.size();
}

} // namespace gb28181
