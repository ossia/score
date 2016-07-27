#pragma once
#include <OSSIA/Protocols/v2/OSSIADevice_v2.hpp>

namespace Ossia
{
namespace Protocols
{
class MinuitDevice final : public OSSIADevice_v2
{
    public:
        MinuitDevice(const Device::DeviceSettings& settings);

        bool reconnect() override;
};
}
}
