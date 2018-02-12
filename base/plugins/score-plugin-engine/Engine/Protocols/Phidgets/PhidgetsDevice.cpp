// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>

#include "PhidgetsDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/phidgets/phidgets_protocol.hpp>
#include <ossia/network/phidgets/phidgets_device.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Phidgets/PhidgetsSpecificSettings.hpp>
#include <Explorer/DeviceList.hpp>

namespace Engine
{
namespace Network
{
PhidgetDevice::PhidgetDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  connect(this, &PhidgetDevice::sig_command,
          this, &PhidgetDevice::slot_command, Qt::QueuedConnection);

  reconnect();

}

bool PhidgetDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& stgs = settings().deviceSpecificSettings.value<PhidgetSpecificSettings>();

    m_dev = std::make_unique<ossia::phidget_device>(settings().name.toStdString());
    static_cast<ossia::phidget_protocol&>(m_dev->get_protocol())
        .set_command_callback([=] { sig_command(); });

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

void PhidgetDevice::slot_command()
{
  if(m_dev)
  {
    auto proto = dynamic_cast<ossia::phidget_protocol*>(&m_dev->get_protocol());
    proto->run_commands();
  }
}
}
}
