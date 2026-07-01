#ifndef GB28181_CORE_SIPENDPOINT_H
#define GB28181_CORE_SIPENDPOINT_H

#include <string>

namespace gb28181 {

struct SipListenEndpoint
{
    std::string name;
    std::string sipId;
    std::string ip;
    int port;
};

} // namespace gb28181

#endif
