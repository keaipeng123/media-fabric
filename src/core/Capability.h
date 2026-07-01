#ifndef GB28181_CORE_CAPABILITY_H
#define GB28181_CORE_CAPABILITY_H

#include <string>

namespace gb28181 {

struct NodeRuntime;

class Capability
{
public:
    virtual ~Capability() {}

    virtual std::string name() const = 0;
    virtual bool start(NodeRuntime& runtime) = 0;
    virtual void stop() = 0;
};

} // namespace gb28181

#endif
