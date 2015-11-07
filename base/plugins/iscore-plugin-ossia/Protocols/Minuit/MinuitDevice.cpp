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
                                 stgs.inPort,
                                 stgs.outPort);
}()
}
{
    reconnect();
}

bool MinuitDevice::reconnect()
{
    m_dev.reset();
    m_connected = false;

    try {
        m_dev = OSSIA::Device::create(m_minuitSettings, settings().name.toStdString());
        m_connected = true;
    }
    catch(...)
    {
        // TODO save the reason of the non-connection.
    }

    return m_connected;
}

void MinuitDevice::updateSettings(const iscore::DeviceSettings& settings)
{
    ISCORE_TODO;
    disconnect();

    m_settings = settings;
    m_dev->setName(m_settings.name.toStdString());
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();

    // TODO m_dev->setName(m_settings.name.toStdString());

    auto prot = dynamic_cast<OSSIA::Minuit*>(m_dev->getProtocol().get());
    prot->setInPort(stgs.inPort);
    prot->setOutPort(stgs.outPort);
    prot->setIp(stgs.host.toStdString());

    reconnect();
}

bool MinuitDevice::canRefresh() const
{
    return true;
}
