#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace Ossia
{
namespace Protocols
{
class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;
};
}
}
