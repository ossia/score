// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>

#include "SerialDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia-qt/serial/serial_protocol.hpp>
#include <ossia-qt/serial/serial_device.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Serial/SerialSpecificSettings.hpp>

#include <Explorer/DeviceList.hpp>
namespace Engine
{
namespace Network
{
SerialDevice::SerialDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
}

bool SerialDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& stgs = settings().deviceSpecificSettings.value<SerialSpecificSettings>();

    m_dev = std::make_unique<ossia::net::serial_device>(
                stgs.text.toUtf8(), stgs.port);

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
