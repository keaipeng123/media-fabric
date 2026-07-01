#ifndef GB28181_CORE_NODERUNTIME_H
#define GB28181_CORE_NODERUNTIME_H

namespace gb28181 {

class NodeConfig;
class BusinessState;
class PeerRegistry;
class SessionManager;
class SipStack;
class TaskScheduler;
class MediaManager;

struct NodeRuntime
{
    NodeConfig* config;
    BusinessState* businessState;
    SipStack* sipStack;
    PeerRegistry* peerRegistry;
    SessionManager* sessionManager;
    TaskScheduler* taskScheduler;
    MediaManager* mediaManager;
};

} // namespace gb28181

#endif
