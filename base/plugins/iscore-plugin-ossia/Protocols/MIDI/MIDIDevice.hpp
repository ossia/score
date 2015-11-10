#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MIDISpecificSettings.hpp"
class MIDIDevice final : public OSSIADevice
{
    public:
        MIDIDevice(const iscore::DeviceSettings& settings);

        bool reconnect() override;
};
