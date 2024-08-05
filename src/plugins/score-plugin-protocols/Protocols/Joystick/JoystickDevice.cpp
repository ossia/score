#include "JoystickDevice.hpp"

#include "JoystickSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/protocols/joystick/game_controller_protocol.hpp>
#include <ossia/protocols/joystick/joystick_protocol.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::JoystickDevice)

namespace Protocols
{

JoystickDevice::JoystickDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

JoystickDevice::~JoystickDevice() { }

template <typename T>
void JoystickDevice::do_reconnect(JoystickSpecificSettings& stgs)
{
  try
  {
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::make_unique<T>(m_ctx, stgs.spec.first, stgs.spec.second),
        settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::runtime_error&)
  {
    // Maybe the joystick has become unavailable. Try to find a similar available one just once.
    try
    {
      stgs.spec = ossia::net::joystick_info::get_available_id_for_uid(stgs.id.data);
      if(stgs.spec != stgs.unassigned)
      {
        m_dev = std::make_unique<ossia::net::generic_device>(
            std::make_unique<T>(m_ctx, stgs.spec.first, stgs.spec.second),
            settings().name.toStdString());

        // update the settings with the new spec...
        this->m_settings.deviceSpecificSettings = QVariant::fromValue(stgs);

        deviceChanged(nullptr, m_dev.get());
      }
    }
    catch(const std::runtime_error&)
    {
      throw;
    }
  }
}
bool JoystickDevice::reconnect()
{
  disconnect();

  auto stgs = settings().deviceSpecificSettings.value<JoystickSpecificSettings>();
  try
  {
    if(stgs.gamepad)
    {
      try
      {
        do_reconnect<ossia::net::game_controller_protocol>(stgs);
      }
      catch(...)
      {
        do_reconnect<ossia::net::joystick_protocol>(stgs);
      }
    }
    else
    {
      do_reconnect<ossia::net::joystick_protocol>(stgs);
    }
  }
  catch(...)
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
