
#include "JoystickDevice.hpp"

#include "JoystickSpecificSettings.hpp"

#include <ossia/network/joystick/joystick_protocol.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::JoystickDevice)

namespace Protocols
{

JoystickDevice::JoystickDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

JoystickDevice::~JoystickDevice() { }

bool JoystickDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs = settings().deviceSpecificSettings.value<JoystickSpecificSettings>();

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::joystick_protocol>(stgs.joystick_id, stgs.joystick_index),
        settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());
  }
  catch (...)
  {
    SCORE_TODO;
  }

  return connected();
}

void JoystickDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
