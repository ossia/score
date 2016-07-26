#pragma once
#include <OSSIA/Protocols/v2/OSSIADevice_v2.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

namespace Ossia
{
namespace Protocols
{
class OSCDevice final : public OSSIADevice_v2
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;
};
}
}
