#include <Network/Device.h>
#include <QDebug>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "Network/Protocol/OSC.h"
#include "OSCDevice.hpp"
#include <OSSIA/Protocols/OSC/OSCSpecificSettings.hpp>

namespace Ossia
{
OSCDevice::OSCDevice(const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool OSCDevice::reconnect()
{
    OSSIADevice::disconnect();

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
}
