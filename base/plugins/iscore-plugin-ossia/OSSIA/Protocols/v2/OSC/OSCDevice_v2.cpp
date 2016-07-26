#include <Network/Device.h>
#include <QDebug>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "BasicOSC.h"
#include "BasicDevice.h"
#include "OSCDevice_v2.hpp"
#include <OSSIA/Protocols/OSC/OSCSpecificSettings.hpp>

namespace Ossia
{
namespace Protocols
{
OSCDevice::OSCDevice(const Device::DeviceSettings &settings):
    OSSIADevice_v2{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool OSCDevice::reconnect()
{
    disconnect();

    try {
        auto stgs = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
        std::unique_ptr<OSSIA::v2::Protocol> ossia_settings = std::make_unique<impl::OSC2>(stgs.host.toStdString(),
                                      stgs.inputPort,
                                      stgs.outputPort);
        m_dev = std::make_unique<impl::BasicDevice>(
                    std::move(ossia_settings),
                    settings().name.toStdString());
        setLogging_impl(isLogging());
    }
    catch(...)
    {
        ISCORE_TODO;
    }

    return connected();
}
}
}
