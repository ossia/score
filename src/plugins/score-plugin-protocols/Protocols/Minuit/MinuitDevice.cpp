// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MinuitDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>

#include <Protocols/Minuit/MinuitSpecificSettings.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/minuit/minuit.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <QDebug>

#include <wobjectimpl.h>

#include <memory>

namespace Protocols
{
MinuitDevice::MinuitDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.hasCallbacks = false;
}

bool MinuitDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs = settings().deviceSpecificSettings.value<MinuitSpecificSettings>();

    std::unique_ptr<ossia::net::protocol_base> ossia_settings
        = std::make_unique<ossia::net::minuit_protocol>(
            stgs.localName.toStdString(), stgs.host.toStdString(), stgs.inputPort,
            stgs.outputPort);

    if(stgs.rate)
    {
      ossia::net::rate_limiter_configuration conf{
          .duration = std::chrono::milliseconds(*stgs.rate)};
      ossia_settings = std::make_unique<ossia::net::rate_limiting_protocol>(
          conf, std::move(ossia_settings));
    }

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());

    setLogging_impl(Device::get_cur_logging(isLogging()));
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

void MinuitDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}
}
