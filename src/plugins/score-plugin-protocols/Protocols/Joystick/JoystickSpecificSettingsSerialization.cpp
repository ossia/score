#include "JoystickSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>

template <>
void DataStreamReader::read(const Protocols::JoystickSpecificSettings& n)
{
  m_stream << n.id;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::JoystickSpecificSettings& n)
{
  m_stream >> n.id;
  n.spec = ossia::net::joystick_info::get_available_id_for_uid(n.id.data);
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::JoystickSpecificSettings& n)
{
  obj["Id"] = n.id;
}

template <>
void JSONWriter::write(Protocols::JoystickSpecificSettings& n)
{
  n.id <<= obj["Id"];
  n.spec = ossia::net::joystick_info::get_available_id_for_uid(n.id.data);
}
