#ifndef GB28181_CORE_BUSINESSQUERYSERVICE_H
#define GB28181_CORE_BUSINESSQUERYSERVICE_H

#include "BusinessState.h"

#include <string>

namespace gb28181 {

class BusinessQueryService
{
public:
    explicit BusinessQueryService(const BusinessState& state);

    std::string summaryJson() const;
    std::string catalogJson(const std::string& peerId) const;
    std::string recordJson(const std::string& peerId) const;

private:
    const BusinessState& m_state;
};

} // namespace gb28181

#endif
