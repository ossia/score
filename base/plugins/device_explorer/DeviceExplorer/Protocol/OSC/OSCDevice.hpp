#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include "OSCSpecificSettings.hpp"
//#include <API/Headers/Network/Protocols.h>
//#include <API/Headers/Network/Device.h>
//#include <API/Headers/Network/Address.h>
class OSCDevice : public DeviceInterface
{
    public:
        OSCDevice(const OSCSpecificSettings& settings)
        {
            //using namespace OSSIA;
            //OSC oscDeviceParameters{settings.host, settings.inputPort, settings.outputPort};

        }
};
