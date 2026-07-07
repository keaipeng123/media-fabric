#ifndef GB28181_CORE_SIPREQUESTCONTEXT_H
#define GB28181_CORE_SIPREQUESTCONTEXT_H

#include "DigestAuth.h"
#include "ManscdpMessage.h"

#include <string>

namespace gb28181 {

struct SipRequestContext
{
    std::string method;
    std::string event;
    std::string fromId;
    std::string toId;
    std::string callId;
    std::string cseq;
    std::string contact;
    std::string fromTag;
    std::string toTag;
    std::string body;
    ManscdpMessage manscdp;
    DigestAuthFields digestAuth;
    int statusCode;
    std::string reason;
    int expires;
    bool authenticated;

    SipRequestContext()
        : statusCode(0),
          expires(0),
          authenticated(false)
    {
    }
};

} // namespace gb28181

#endif
