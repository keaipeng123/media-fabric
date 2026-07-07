#ifndef GB28181_CORE_DIGESTAUTH_H
#define GB28181_CORE_DIGESTAUTH_H

#include <string>

namespace gb28181 {

struct DigestAuthFields
{
    std::string username;
    std::string realm;
    std::string nonce;
    std::string uri;
    std::string response;
    std::string algorithm;
    std::string opaque;
    std::string qop;
    std::string nc;
    std::string cnonce;
};

std::string md5Hex(const std::string& input);
std::string computeDigestResponse(const std::string& method,
                                  const std::string& username,
                                  const std::string& realm,
                                  const std::string& password,
                                  const std::string& nonce,
                                  const std::string& uri,
                                  const std::string& qop,
                                  const std::string& nc,
                                  const std::string& cnonce);
bool verifyDigestResponse(const std::string& method,
                          const DigestAuthFields& fields,
                          const std::string& expectedUsername,
                          const std::string& expectedRealm,
                          const std::string& password);

} // namespace gb28181

#endif
