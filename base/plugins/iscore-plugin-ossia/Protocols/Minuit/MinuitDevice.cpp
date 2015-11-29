#include <API/Headers/Network/Device.h>
#include <QString>
#include <QVariant>
#include <memory>

#include "Device/Protocol/DeviceSettings.hpp"
#include "MinuitDevice.hpp"
#include "Network/Protocol/Minuit.h"
#include "Protocols/Minuit/MinuitSpecificSettings.hpp"

MinuitDevice::MinuitDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{

    reconnect();
}

bool MinuitDevice::reconnect()
{
    m_dev.reset();

    try {
        auto stgs = settings().deviceSpecificSettings.value<MinuitSpecificSettings>();
        auto ossia_settings = OSSIA::Minuit::create(
                               stgs.host.toStdString(),
                               stgs.inputPort,
                               stgs.outputPort);

        m_dev = OSSIA::Device::create(ossia_settings, settings().name.toStdString());
    }
    catch(...)
    {
        // TODO save the reason of the non-connection.
    }

    return connected();
}

bool MinuitDevice::canRefresh() const
{
    return true;
}
