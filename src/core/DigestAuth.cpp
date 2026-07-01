#include "DigestAuth.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace gb28181 {
namespace {

uint32_t leftRotate(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

uint32_t loadLe32(const unsigned char* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

void storeLe32(uint32_t value, unsigned char* data)
{
    data[0] = static_cast<unsigned char>(value & 0xff);
    data[1] = static_cast<unsigned char>((value >> 8) & 0xff);
    data[2] = static_cast<unsigned char>((value >> 16) & 0xff);
    data[3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

std::string toLowerCopy(const std::string& value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

std::string md5Hex(const std::string& input)
{
    static const uint32_t shifts[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    static const uint32_t constants[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

    std::vector<unsigned char> message(input.begin(), input.end());
    const uint64_t bitLength = static_cast<uint64_t>(message.size()) * 8;
    message.push_back(0x80);
    while ((message.size() % 64) != 56)
    {
        message.push_back(0);
    }
    for (int i = 0; i < 8; ++i)
    {
        message.push_back(static_cast<unsigned char>((bitLength >> (8 * i)) & 0xff));
    }

    uint32_t a0 = 0x67452301;
    uint32_t b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe;
    uint32_t d0 = 0x10325476;

    for (size_t offset = 0; offset < message.size(); offset += 64)
    {
        uint32_t words[16];
        for (int i = 0; i < 16; ++i)
        {
            words[i] = loadLe32(&message[offset + i * 4]);
        }

        uint32_t a = a0;
        uint32_t b = b0;
        uint32_t c = c0;
        uint32_t d = d0;

        for (uint32_t i = 0; i < 64; ++i)
        {
            uint32_t f = 0;
            uint32_t g = 0;
            if (i < 16)
            {
                f = (b & c) | ((~b) & d);
                g = i;
            }
            else if (i < 32)
            {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            }
            else
            {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }

            const uint32_t temp = d;
            d = c;
            c = b;
            b = b + leftRotate(a + f + constants[i] + words[g], shifts[i]);
            a = temp;
        }

        a0 += a;
        b0 += b;
        c0 += c;
        d0 += d;
    }

    unsigned char digest[16];
    storeLe32(a0, digest);
    storeLe32(b0, digest + 4);
    storeLe32(c0, digest + 8);
    storeLe32(d0, digest + 12);

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = 0; i < sizeof(digest); ++i)
    {
        output << std::setw(2) << static_cast<int>(digest[i]);
    }
    return output.str();
}

std::string computeDigestResponse(const std::string& method,
                                  const std::string& username,
                                  const std::string& realm,
                                  const std::string& password,
                                  const std::string& nonce,
                                  const std::string& uri,
                                  const std::string& qop,
                                  const std::string& nc,
                                  const std::string& cnonce)
{
    const std::string ha1 = md5Hex(username + ":" + realm + ":" + password);
    const std::string ha2 = md5Hex(method + ":" + uri);
    if (!qop.empty())
    {
        return md5Hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
    }
    return md5Hex(ha1 + ":" + nonce + ":" + ha2);
}

bool verifyDigestResponse(const std::string& method,
                          const DigestAuthFields& fields,
                          const std::string& expectedUsername,
                          const std::string& expectedRealm,
                          const std::string& password)
{
    if (fields.username.empty() || fields.realm.empty() || fields.nonce.empty() ||
        fields.uri.empty() || fields.response.empty() || password.empty())
    {
        return false;
    }
    if (!expectedUsername.empty() && fields.username != expectedUsername)
    {
        return false;
    }
    if (fields.realm != expectedRealm)
    {
        return false;
    }
    if (!fields.algorithm.empty() && toLowerCopy(fields.algorithm) != "md5")
    {
        return false;
    }

    const std::string expected = computeDigestResponse(method,
                                                       fields.username,
                                                       fields.realm,
                                                       password,
                                                       fields.nonce,
                                                       fields.uri,
                                                       fields.qop,
                                                       fields.nc,
                                                       fields.cnonce);
    return toLowerCopy(fields.response) == expected;
}

} // namespace gb28181
