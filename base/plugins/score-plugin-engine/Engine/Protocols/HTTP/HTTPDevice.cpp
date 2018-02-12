// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>

#include "HTTPDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia-qt/http/http.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/HTTP/HTTPSpecificSettings.hpp>
#include <Explorer/DeviceList.hpp>

namespace Engine
{
namespace Network
{
HTTPDevice::HTTPDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
  reconnect();
}

bool HTTPDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs
        = settings().deviceSpecificSettings.value<HTTPSpecificSettings>();

    m_dev = std::make_unique<ossia::net::http_device>(
        std::make_unique<ossia::net::http_protocol>(stgs.text.toUtf8()),
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
