#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/v2/generic/generic_device.hpp>
#include <ossia/network/v2/generic/generic_address.hpp>
#include <ossia/network/v2/minuit/minuit.hpp>
#include "MinuitDevice_v2.hpp"
#include <OSSIA/Protocols/Minuit/MinuitSpecificSettings.hpp>

namespace Ossia
{
namespace Protocols
{
MinuitDevice::MinuitDevice(const Device::DeviceSettings &settings):
    OSSIADevice_v2{settings}
{
    m_capas.canRefreshTree = true;

    reconnect();
}

bool MinuitDevice::reconnect()
{
    disconnect();

    try {
        auto stgs = settings().deviceSpecificSettings.value<MinuitSpecificSettings>();

        std::unique_ptr<OSSIA::v2::Protocol> ossia_settings = std::make_unique<impl::Minuit2>(
                    stgs.host.toStdString(),
                    stgs.inputPort,
                    stgs.outputPort);

        m_dev = std::make_unique<impl::BasicDevice>(
              std::move(ossia_settings),
              settings().name.toStdString());

        setLogging_impl(isLogging());
    }
    catch(std::exception& e)
    {
        qDebug() << "Could not connect: " << e.what();
    }
    catch(...)
    {
        // TODO save the reason of the non-connection.
    }

    return connected();
}
}
}
