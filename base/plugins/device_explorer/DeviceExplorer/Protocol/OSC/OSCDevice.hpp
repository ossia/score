#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include "OSCSpecificSettings.hpp"
//#include <API/Headers/Network/Protocol.h>
//#include <API/Headers/Network/Device.h>
//#include <API/Headers/Network/Address.h>
class OSCDevice : public DeviceInterface
{
        //std::shared_ptr<OSSIA::Device> m_device{};
    public:
        OSCDevice(const DeviceSettings& stngs):
            DeviceInterface{stngs}
        {
            //auto settings = stngs.deviceSpecificSettings.value<OSCSpecificSettings>();
            //OSSIA::OSC oscDeviceParameter{settings.host.toStdString(), settings.inputPort, settings.outputPort};
            //m_device = OSSIA::Device::create(oscDeviceParameter);
        }

        virtual void addPath(const AddressSettings& address) override
        {
            //auto node = m_device->emplace(m_device->begin(), address.path.toStdString());
            //node->createAddress(static_cast<OSSIA::AddressValue::Type>(address.type));
        }

        virtual void removePath(const QString& path) override
        {

        }

        virtual void sendMessage(Message& mess) override
        {
            qDebug() << mess.address << mess.value;
        }

        virtual bool check(const QString& str) override
        {
            return false;
        }
};
