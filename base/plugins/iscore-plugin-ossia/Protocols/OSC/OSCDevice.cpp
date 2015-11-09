#include "OSCDevice.hpp"
#include <API/Headers/Network/Device.h>
#include <iscore2OSSIA.hpp>
#include <OSSIA2iscore.hpp>

OSCDevice::OSCDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings},
    m_oscSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<OSCSpecificSettings>();
    return OSSIA::OSC::create(stgs.host.toStdString(),
                              stgs.inputPort,
                              stgs.outputPort);
}()}
{
    using namespace OSSIA;

    reconnect();
}

bool OSCDevice::reconnect()
{
    m_dev.reset();

    try {
        m_dev = OSSIA::Device::create(m_oscSettings, settings().name.toStdString());
    }
    catch(...)
    {
        ISCORE_TODO;
    }

    return connected();
}

void OSCDevice::updateOSSIASettings()
{
    auto stgs = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
    auto prot = dynamic_cast<OSSIA::OSC*>(m_oscSettings.get());
    prot->setInPort(stgs.inputPort);
    prot->setOutPort(stgs.outputPort);
    prot->setIp(stgs.host.toStdString());
}
