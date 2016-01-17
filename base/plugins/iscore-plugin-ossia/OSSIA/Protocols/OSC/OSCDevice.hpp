#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

namespace Ossia
{
class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;
};
}
