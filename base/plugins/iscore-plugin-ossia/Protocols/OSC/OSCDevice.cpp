#include "OSCDevice.hpp"
#include <API/Headers/Network/Device.h>

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

void OSCDevice::updateSettings(const iscore::DeviceSettings& settings)
{
    // TODO save the node, else we lose it.
    // Also, we have to maintain the prior connection state
    // if we were disconnected, we stay disconnected
    // else we reconnect. See in Minuit / MIDI also.
    ISCORE_TODO;
    disconnect();

    m_settings = settings;
    m_dev->setName(m_settings.name.toStdString());
    auto stgs = settings.deviceSpecificSettings.value<OSCSpecificSettings>();

    m_dev->setName(m_settings.name.toStdString());

    auto prot = dynamic_cast<OSSIA::OSC*>(m_dev->getProtocol().get());
    prot->setInPort(stgs.inputPort);
    prot->setOutPort(stgs.outputPort);
    prot->setIp(stgs.host.toStdString());

    reconnect();
}

bool OSCDevice::reconnect()
{
    m_dev.reset();
    m_connected = false;

    try {
        m_dev = OSSIA::Device::create(m_oscSettings, settings().name.toStdString());
        m_connected = true;
    }
    catch(...)
    {
        ISCORE_TODO;
    }

    return m_connected;
}
