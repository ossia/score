#include "JoystickSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>

namespace
{
// Initializing the joystick subsystem throws where it is unavailable (SDL
// without joystick/haptic support, e.g. WebAssembly). Loading settings must not
// depend on it: leave the joystick unassigned instead.
std::pair<int32_t, int32_t> resolve_joystick_spec(const score::uuid_t& id) noexcept
{
  try
  {
    return ossia::net::joystick_info::get_available_id_for_uid(id.data);
  }
  catch(...)
  {
    return Protocols::JoystickSpecificSettings::unassigned;
  }
}
}

// Note: JoystickSpecificSettings.spec is not meant to be serialized
template <>
void DataStreamReader::read(const Protocols::JoystickSpecificSettings& n)
{
  m_stream << n.id << n.gamepad;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::JoystickSpecificSettings& n)
{
  m_stream >> n.id >> n.gamepad;
  n.spec = resolve_joystick_spec(n.id);
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::JoystickSpecificSettings& n)
{
  obj["Id"] = n.id;
  obj["Gamepad"] = n.gamepad;
}

template <>
void JSONWriter::write(Protocols::JoystickSpecificSettings& n)
{
  n.id <<= obj["Id"];
  if(auto gp = obj.tryGet("Gamepad"))
    n.gamepad = gp->toBool();

  n.spec = resolve_joystick_spec(n.id);
}
