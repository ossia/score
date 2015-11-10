#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Editor/Value.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

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
