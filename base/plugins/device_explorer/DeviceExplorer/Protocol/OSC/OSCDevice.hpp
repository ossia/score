#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include "OSCSpecificSettings.hpp"
//#include <API/Headers/Network/Protocols.h>
//#include <API/Headers/Network/Device.h>
//#include <API/Headers/Network/Address.h>
class OSCDevice : public DeviceInterface
{
 //       OSSIA::OSC m_device;
    public:
        OSCDevice(const OSCSpecificSettings& settings)
            //: m_device{settings.host, settings.inputPort, settings.outputPort}
        {
        }

        virtual void addPath(const AddressSettings& address) override
        {
            //auto node = m_device->emplace(std::begin(m_device), address.path);
            //node->createAddress(address.type);
        }

        virtual void removePath(const QString& path) override
        {

        }

        virtual void sendMessage(Message& mess) override
        {

        }

        virtual bool check(const QString& str) override
        {
            return false;
        }
};
