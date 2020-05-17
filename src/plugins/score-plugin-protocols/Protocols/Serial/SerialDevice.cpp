// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Protocols/Serial/SerialSpecificSettings.hpp>

#include <ossia-qt/serial/serial_protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>

#include <memory>

namespace Protocols
{
SerialDevice::SerialDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
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
        std::make_unique<ossia::net::serial_protocol>(stgs.text.toUtf8(), stgs.port),
        settings().name.toStdString());

    deviceChanged(nullptr, m_dev.get());

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
#endif
