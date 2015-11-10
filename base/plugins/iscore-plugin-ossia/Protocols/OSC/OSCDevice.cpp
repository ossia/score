#include "OSCDevice.hpp"
#include <API/Headers/Network/Device.h>
#include <iscore2OSSIA.hpp>
#include <OSSIA2iscore.hpp>

OSCDevice::OSCDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool OSCDevice::reconnect()
{
    m_dev.reset();

    try {
        auto stgs = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
        auto ossia_settings = OSSIA::OSC::create(stgs.host.toStdString(),
                                      stgs.inputPort,
                                      stgs.outputPort);
        m_dev = OSSIA::Device::create(ossia_settings, settings().name.toStdString());
    }
    catch(...)
    {
        ISCORE_TODO;
    }

    return connected();
}
