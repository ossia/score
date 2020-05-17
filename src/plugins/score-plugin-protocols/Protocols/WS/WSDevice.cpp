// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WSDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Protocols/WS/WSSpecificSettings.hpp>

#include <ossia-qt/websocket-generic-client/ws_generic_client_protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>

#include <memory>

namespace Protocols
{
WSDevice::WSDevice(const Device::DeviceSettings& settings) : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
}

bool WSDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs = settings().deviceSpecificSettings.value<WSSpecificSettings>();

    m_dev = std::make_unique<ossia::net::ws_generic_client_device>(
        std::make_unique<ossia::net::ws_generic_client_protocol>(
            stgs.address.toUtf8(), stgs.text.toUtf8()),
        settings().name.toStdString());

    enableCallbacks();

    setLogging_impl(Device::get_cur_logging(isLogging()));

    deviceChanged(nullptr, m_dev.get());
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
}
