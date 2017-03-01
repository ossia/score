#include <QString>
#include <QVariant>
#include <memory>

#include "SerialDevice.hpp"
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia-qt/serial/serial_protocol.hpp>
#include <ossia-qt/serial/serial_device.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Serial/SerialSpecificSettings.hpp>

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
  reconnect();
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
}
}
