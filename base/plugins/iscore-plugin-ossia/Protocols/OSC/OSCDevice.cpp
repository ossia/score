#include "OSCDevice.hpp"
#include <API/Headers/Network/Protocol/OSC.h>
#include <API/Headers/Network/Device.h>

OSCDevice::OSCDevice(const iscore::DeviceSettings &stngs):
    OSSIADevice{stngs}
{
    using namespace OSSIA;

    auto settings = stngs.deviceSpecificSettings.value<OSCSpecificSettings>();
    auto oscDeviceParameter = OSSIA::OSC::create(
                settings.host.toStdString(),
                settings.inputPort,
                settings.outputPort);
    m_dev = OSSIA::Device::create(oscDeviceParameter, stngs.name.toStdString());
}

void OSCDevice::updateSettings(const iscore::DeviceSettings& settings)
{
    m_settings = settings;
    auto stgs = settings.deviceSpecificSettings.value<OSCSpecificSettings>();

    m_dev->setName(m_settings.name.toStdString());

    auto prot = dynamic_cast<OSSIA::OSC*>(m_dev->getProtocol().get());
    prot->setInPort(stgs.inputPort);
    prot->setOutPort(stgs.outputPort);
    prot->setIp(stgs.host.toStdString());
}
