#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MIDISpecificSettings.hpp"
class MIDIDevice final : public OSSIADevice
{
    public:
        MIDIDevice(const iscore::DeviceSettings& settings);

        void updateOSSIASettings() override;
        bool reconnect() override;
};
