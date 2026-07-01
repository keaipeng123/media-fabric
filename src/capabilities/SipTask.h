#ifndef GB28181_CAPABILITIES_SIPTASK_H
#define GB28181_CAPABILITIES_SIPTASK_H

#include "SipRequestContext.h"

#include <string>

namespace gb28181 {

struct NodeRuntime;

class SipTask
{
public:
    virtual ~SipTask() {}

    virtual std::string name() const = 0;
    virtual bool run(const SipRequestContext& request, NodeRuntime& runtime) = 0;
};

} // namespace gb28181

#endif
