#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/wrapped_parameter.hpp>
#include <ossia/network/base/parameter_data.hpp>

#include "avnd_protocol.hpp"

namespace oscr
{
class DeviceImplementation final : public Device::OwningDeviceInterface
{
public:
  DeviceImplementation(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
  {
    m_capas.canRefreshTree = false;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canRenameNode = false;
    m_capas.canSetProperties = false;
    m_capas.canSerialize = false;
  }

  ~DeviceImplementation() {}

  bool reconnect() override
  {
    disconnect();

    try
    {
      auto protocol = std::make_unique<avnd_protocol>();
      auto dev = std::make_unique<ossia::net::generic_device>(
            std::move(protocol), settings().name.toStdString());

      m_dev = std::move(dev);
      deviceChanged(nullptr, m_dev.get());

      enableCallbacks();
    }
    catch (const std::runtime_error& e)
    {
      qDebug() << e.what();
    }
    catch (...)
    {
      qDebug() << "error";
    }

    return connected();
  }

  void disconnect() override
  {
    OwningDeviceInterface::disconnect();
  }
};
}
