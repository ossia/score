#include "MinuitDevice.hpp"
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
MinuitDevice::MinuitDevice(const DeviceSettings &settings):
    DeviceInterface{settings}
{
    using namespace OSSIA;
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    Minuit minuitDeviceParameters(settings.name.toStdString(),  stgs.host.toStdString(), stgs.port);

    //auto prot = Device::create(minuitDeviceParameters, "da");
}

void MinuitDevice::addAddress(const AddressSettings &address)
{

}

void MinuitDevice::updateAddress(const AddressSettings &address)
{

}

void MinuitDevice::removeAddress(const QString &path)
{

}

void MinuitDevice::sendMessage(Message &mess)
{

}

bool MinuitDevice::check(const QString &str)
{
    return false;
}
