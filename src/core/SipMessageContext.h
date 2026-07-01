#ifndef GB28181_CORE_SIPMESSAGECONTEXT_H
#define GB28181_CORE_SIPMESSAGECONTEXT_H

#include <string>

namespace gb28181 {

struct SipMessageContext
{
    std::string method;
    std::string event;
    std::string fromId;
    std::string toId;
    std::string localIp;
    int localPort;
    std::string remoteIp;
    int remotePort;
    std::string body;
    std::string contentType;
    std::string authRealm;
    std::string authNonce;
    std::string authOpaque;
    std::string authAlgorithm;
    int expires;
    bool response;
    int statusCode;
    std::string reason;

    SipMessageContext()
        : localPort(0),
          remotePort(0),
          expires(0),
          response(false),
          statusCode(0)
    {
    }
};

} // namespace gb28181

#endif
