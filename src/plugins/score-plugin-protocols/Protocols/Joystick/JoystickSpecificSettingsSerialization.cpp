#include "JoystickSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::JoystickSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::JoystickSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Protocols::JoystickSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Protocols::JoystickSpecificSettings& n)
{
}
