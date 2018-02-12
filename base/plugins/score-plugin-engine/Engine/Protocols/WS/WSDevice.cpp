// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>

#include "WSDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia-qt/websocket-generic-client/ws_generic_client.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/WS/WSSpecificSettings.hpp>
#include <Explorer/DeviceList.hpp>

namespace Engine
{
namespace Network
{
WSDevice::WSDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  reconnect();
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
}
