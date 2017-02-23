#include <QString>
#include <QVariant>
#include <memory>

#include "MinuitDevice.hpp"
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/minuit/minuit.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Minuit/MinuitSpecificSettings.hpp>

namespace Engine
{
namespace Network
{
MinuitDevice::MinuitDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.hasCallbacks = false;

  reconnect();
}

bool MinuitDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs
        = settings().deviceSpecificSettings.value<MinuitSpecificSettings>();

    std::unique_ptr<ossia::net::protocol_base> ossia_settings
        = std::make_unique<ossia::net::minuit_protocol>(
            stgs.localName.toStdString(),
            stgs.host.toStdString(),
            stgs.inputPort,
            stgs.outputPort);

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());

    setLogging_impl(isLogging());
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

void MinuitDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

}
}
