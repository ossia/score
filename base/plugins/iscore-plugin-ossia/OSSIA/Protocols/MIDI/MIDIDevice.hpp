#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

class MIDIDevice final : public OSSIADevice
{
    public:
        MIDIDevice(const Device::DeviceSettings& settings);

        bool reconnect() override;
};
