#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class OSCDevice final : public OwningOSSIADevice
{
    public:
        OSCDevice(const Device::DeviceSettings& stngs);

        bool reconnect() override;

        bool isLearning() const final override;
        void setLearning(bool) final override;

};
}
}
