#include <API/Headers/Network/Device.h>
#include <qdebug.h>
#include <qstring.h>
#include <qvariant.h>
#include <memory>

#include "Device/Protocol/DeviceSettings.hpp"
#include "Network/Protocol/OSC.h"
#include "OSCDevice.hpp"
#include "Protocols/OSC/OSCSpecificSettings.hpp"

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
