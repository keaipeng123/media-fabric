#ifndef GB28181_CAPABILITIES_STANDARDCAPABILITIES_H
#define GB28181_CAPABILITIES_STANDARDCAPABILITIES_H

#include "Capability.h"

#include <memory>
#include <vector>

namespace gb28181 {

std::vector<std::unique_ptr<Capability> > createStandardCapabilities();

} // namespace gb28181

#endif
