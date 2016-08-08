#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore


namespace Engine
{
namespace Network
{
class MIDIDevice final : public OwningOSSIADevice
{
    public:
        MIDIDevice(const Device::DeviceSettings& settings);

        bool reconnect() override;

        void disconnect() override;

        using OwningOSSIADevice::refresh;
        Device::Node refresh() override;
};
}
}
