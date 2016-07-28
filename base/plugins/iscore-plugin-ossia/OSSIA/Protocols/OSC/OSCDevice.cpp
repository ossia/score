#include <QDebug>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/osc/osc.hpp>
#include "OSCDevice.hpp"
#include <OSSIA/Protocols/OSC/OSCSpecificSettings.hpp>

namespace Ossia
{
namespace Protocols
{
OSCDevice::OSCDevice(const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool OSCDevice::reconnect()
{
    disconnect();

    try {
        auto stgs = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
        std::unique_ptr<OSSIA::net::Protocol> ossia_settings = std::make_unique<impl::OSC2>(
                    stgs.host.toStdString(),
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
