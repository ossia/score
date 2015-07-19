#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MIDISpecificSettings.hpp"
class MIDIDevice : public OSSIADevice
{
    public:
        MIDIDevice(const iscore::DeviceSettings& settings);


};
