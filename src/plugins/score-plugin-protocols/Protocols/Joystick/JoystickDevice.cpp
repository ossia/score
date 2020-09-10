
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

  auto stgs = settings().deviceSpecificSettings.value<JoystickSpecificSettings>();
  try
  {
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::joystick_protocol>(stgs.spec.first, stgs.spec.second),
        settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());
  }
  catch (...)
  {
    // Maybe the joystick has become unavailable. Try to find a similar available one just once.
    try
    {
      stgs.spec = ossia::net::joystick_protocol::get_available_id_for_uid(stgs.id.data);
      if(stgs.spec != stgs.unassigned)
      {
        m_dev = std::make_unique<ossia::net::generic_device>(
            std::make_unique<ossia::net::joystick_protocol>(stgs.spec.first, stgs.spec.second),
            settings().name.toStdString());

        // update the settings with the new spec...
        this->m_settings.deviceSpecificSettings = QVariant::fromValue(stgs);

        deviceChanged(nullptr, m_dev.get());
      }
    }
    catch (...)
    {
      SCORE_TODO;
    }
  }

  return connected();
}

void JoystickDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
