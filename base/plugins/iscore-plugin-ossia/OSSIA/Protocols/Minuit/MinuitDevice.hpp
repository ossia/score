#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace Ossia
{
namespace Protocols
{
class MinuitDevice final : public OwningOSSIADevice
{
    public:
        MinuitDevice(const Device::DeviceSettings& settings);

        bool reconnect() override;
};
}
}
