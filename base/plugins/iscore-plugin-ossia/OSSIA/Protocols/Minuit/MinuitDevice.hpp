#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

namespace OSSIA
{
}
class MinuitDevice final : public OSSIADevice
{
    public:
        MinuitDevice(const iscore::DeviceSettings& settings);

        bool reconnect() override;
};
