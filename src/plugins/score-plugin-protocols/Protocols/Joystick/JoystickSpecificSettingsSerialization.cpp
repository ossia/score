#include "JoystickSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>

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
  n.spec = ossia::net::joystick_info::get_available_id_for_uid(n.id.data);
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

  n.spec = ossia::net::joystick_info::get_available_id_for_uid(n.id.data);
}
