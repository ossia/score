#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class OSCDevice final : public OwningOSSIADevice
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;
};
}
}
