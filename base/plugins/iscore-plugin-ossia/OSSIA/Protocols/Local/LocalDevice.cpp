#include <API/Headers/Network/Device.h>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include "Network/Protocol/Local.h"
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>

LocalDevice::LocalDevice(
        std::shared_ptr<OSSIA::Device> dev,
        const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{
    m_dev = dev;
}

bool LocalDevice::reconnect()
{
    return connected();
}

bool LocalDevice::canRefresh() const
{
    return true;
}
