#ifndef GB28181_CORE_SESSIONMANAGER_H
#define GB28181_CORE_SESSIONMANAGER_H

#include <map>
#include <string>

namespace gb28181 {

struct SessionInfo
{
    std::string id;
    std::string capability;
    std::string peerId;
    std::string state;
};

class SessionManager
{
public:
    bool createSession(const SessionInfo& session);
    bool closeSession(const std::string& sessionId);
    size_t sessionCount() const;

private:
    std::map<std::string, SessionInfo> m_sessions;
};

} // namespace gb28181

#endif
