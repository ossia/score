#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace Ossia
{
namespace Protocols
{
class OSCDevice final : public OwningOSSIADevice
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;
};
}
}
