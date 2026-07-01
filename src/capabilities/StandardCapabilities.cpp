#include "StandardCapabilities.h"

#include "GB28181Capabilities.h"

#include <memory>

namespace gb28181 {

std::vector<std::unique_ptr<Capability> > createStandardCapabilities()
{
    std::vector<std::unique_ptr<Capability> > capabilities;

    capabilities.push_back(std::unique_ptr<Capability>(new RegisterClientCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new RegisterServerCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new KeepaliveClientCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new KeepaliveServerCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new CatalogClientCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new CatalogServerCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new RecordQueryClientCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new RecordQueryServerCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new InviteClientCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new InviteServerCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new MediaReceiveCapability()));
    capabilities.push_back(std::unique_ptr<Capability>(new MediaSendCapability()));

    return capabilities;
}

} // namespace gb28181
