#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include "MinuitSpecificSettings.hpp"
//#include <API/Headers/Network/Protocols.h>
//#include <API/Headers/Network/Device.h>
//#include <API/Headers/Network/Address.h>
class MinuitDevice : public DeviceInterface
{
    public:
        MinuitDevice(const MinuitSpecificSettings& settings)
        {
            //using namespace OSSIA;
            //OSC oscDeviceParameters{settings.host, settings.inputPort, settings.outputPort};

        }
};
