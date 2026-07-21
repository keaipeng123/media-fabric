#include "LifecycleCapability.h"

#include "MediaManager.h"
#include "NodeRuntime.h"
#include "PeerRegistry.h"
#include "SessionManager.h"
#include "SipRequestContext.h"
#include "SipStack.h"
#include "TaskScheduler.h"
#include "Logger.h"

#include <sstream>

namespace gb28181 {

LifecycleCapability::LifecycleCapability(const std::string& capabilityName)
    : m_name(capabilityName),
      m_started(false)
{
}

LifecycleCapability::~LifecycleCapability()
{
    stop();
}

std::string LifecycleCapability::name() const
{
    return m_name;
}

bool LifecycleCapability::start(NodeRuntime& runtime)
{
    if (m_started)
    {
        return true;
    }

    if (runtime.config == NULL || runtime.sipStack == NULL || runtime.peerRegistry == NULL ||
        runtime.sessionManager == NULL || runtime.taskScheduler == NULL || runtime.mediaManager == NULL)
    {
        return false;
    }

    if (!onStart(runtime))
    {
        return false;
    }

    m_started = true;
    return true;
}

void LifecycleCapability::stop()
{
    if (!m_started)
    {
        return;
    }

    onStop();
    m_started = false;
}

bool LifecycleCapability::started() const
{
    return m_started;
}

bool LifecycleCapability::onStart(NodeRuntime& runtime)
{
    std::ostringstream detail;
    detail << "capability=" << name() << " sip_endpoints=" << runtime.sipStack->endpointCount()
           << " peers=" << runtime.peerRegistry->peers().size()
           << " sessions=" << runtime.sessionManager->sessionCount()
           << " scheduled_tasks=" << runtime.taskScheduler->taskCount()
           << " sip_routes=" << runtime.sipStack->routeCount()
           << " rtp_ports=" << runtime.mediaManager->availablePortCount();
    media_fabric::Logger::instance().log(media_fabric::LOG_DEBUG, "capability", detail.str());
    return true;
}

void LifecycleCapability::onStop()
{
}

bool LifecycleCapability::handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime)
{
    SessionInfo session;
    session.id = name() + ":" + request.method + ":" + request.event;
    session.capability = name();
    session.peerId = request.fromId.empty() ? request.toId : request.fromId;
    session.state = "handled";
    runtime.sessionManager->createSession(session);
    return true;
}

bool LifecycleCapability::registerSipHandler(NodeRuntime& runtime, const std::string& method, const std::string& event)
{
    return runtime.sipStack->registerHandler(
        method,
        event,
        name(),
        [this, &runtime](const SipRequestContext& request) {
            return handleSipRequest(request, runtime);
        });
}

} // namespace gb28181
