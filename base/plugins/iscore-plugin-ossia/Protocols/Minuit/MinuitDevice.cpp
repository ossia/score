#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Editor/Value.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

MinuitDevice::MinuitDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings},
    m_minuitSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return OSSIA::Minuit::create(stgs.host.toStdString(),
                                 stgs.inputPort,
                                 stgs.outputPort);
}()
}
{
    reconnect();
}

bool MinuitDevice::reconnect()
{
    m_dev.reset();

    try {
        m_dev = OSSIA::Device::create(m_minuitSettings, settings().name.toStdString());
    }
    catch(...)
    {
        // TODO save the reason of the non-connection.
    }

    return connected();
}

void MinuitDevice::updateOSSIASettings()
{
    auto stgs = settings().deviceSpecificSettings.value<MinuitSpecificSettings>();

    auto prot = dynamic_cast<OSSIA::Minuit*>(m_minuitSettings.get());
    prot->setInPort(stgs.inputPort);
    prot->setOutPort(stgs.outputPort);
    prot->setIp(stgs.host.toStdString());
}

bool MinuitDevice::canRefresh() const
{
    return true;
}
