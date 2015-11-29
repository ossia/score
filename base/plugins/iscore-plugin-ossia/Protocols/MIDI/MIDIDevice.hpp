#pragma once
#include "Protocols/OSSIADevice.hpp"

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

class MIDIDevice final : public OSSIADevice
{
    public:
        MIDIDevice(const iscore::DeviceSettings& settings);

        bool reconnect() override;
};
