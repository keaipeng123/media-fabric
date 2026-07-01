#ifndef GB28181_CAPABILITIES_LIFECYCLECAPABILITY_H
#define GB28181_CAPABILITIES_LIFECYCLECAPABILITY_H

#include "Capability.h"

#include <string>

namespace gb28181 {

struct SipRequestContext;

class LifecycleCapability : public Capability
{
public:
    explicit LifecycleCapability(const std::string& capabilityName);
    virtual ~LifecycleCapability();

    std::string name() const;
    bool start(NodeRuntime& runtime);
    void stop();
    bool started() const;

protected:
    virtual bool onStart(NodeRuntime& runtime);
    virtual void onStop();
    virtual bool handleSipRequest(const SipRequestContext& request, NodeRuntime& runtime);

    bool registerSipHandler(NodeRuntime& runtime, const std::string& method, const std::string& event);

private:
    std::string m_name;
    bool m_started;
};

} // namespace gb28181

#endif
