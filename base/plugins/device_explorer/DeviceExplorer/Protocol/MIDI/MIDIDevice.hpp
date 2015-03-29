#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include "MIDISpecificSettings.hpp"
//#include <API/Headers/Network/Protocols.h>
//#include <API/Headers/Network/Device.h>
//#include <API/Headers/Network/Address.h>
class MIDIDevice : public DeviceInterface
{
    public:
        MIDIDevice(const MIDISpecificSettings& settings)
        {
            //using namespace OSSIA;
            //MIDI MIDIDeviceParameters{settings.host, settings.inputPort, settings.outputPort};

        }
};
