#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIOSpecificSettings.hpp"

#include <score/application/ApplicationComponents.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

JSON_METADATA(Protocols::SimpleIO::GPIO, "GPIO")
JSON_METADATA(Protocols::SimpleIO::PWM, "PWM")
JSON_METADATA(Protocols::SimpleIO::ADC, "ADC")
JSON_METADATA(Protocols::SimpleIO::DAC, "DAC")
JSON_METADATA(Protocols::SimpleIO::HID, "HID")
JSON_METADATA(Protocols::SimpleIO::Custom, "Custom")

template <>
void DataStreamReader::read(const Protocols::SimpleIO::GPIO& n)
{
  m_stream << n.chip << n.direction << n.events << n.flags << n.line << n.state;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::GPIO& n)
{
  m_stream >> n.chip >> n.direction >> n.events >> n.flags >> n.line >> n.state;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::GPIO& n)
{
  stream.StartObject();
  obj["Chip"] = n.chip;
  obj["Line"] = n.line;
  obj["Flags"] = n.flags;
  obj["Events"] = n.events;
  obj["State"] = n.state;
  obj["Direction"] = n.direction;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::GPIO& n)
{
  if(!obj.tryGet("Chip"))
    return;

  n.chip = obj["Chip"].toInt();
  n.line = obj["Line"].toInt();
  n.flags = obj["Flags"].toInt();
  n.events = obj["Events"].toInt();
  n.state = obj["State"].toInt();
  n.direction = obj["Direction"].toBool();
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::ADC& n)
{
  m_stream << n.chip << n.channel;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::ADC& n)
{
  m_stream >> n.chip >> n.channel;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::ADC& n)
{
  stream.StartObject();
  obj["Chip"] = n.chip;
  obj["Channel"] = n.channel;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::ADC& n)
{
  if(!obj.tryGet("Chip"))
    return;

  n.chip = obj["Chip"].toInt();
  n.channel = obj["Channel"].toInt();
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::DAC& n)
{
  m_stream << n.chip << n.channel;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::DAC& n)
{
  m_stream >> n.chip >> n.channel;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::DAC& n)
{
  stream.StartObject();
  obj["Chip"] = n.chip;
  obj["Channel"] = n.channel;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::DAC& n)
{
  if(!obj.tryGet("Chip"))
    return;

  n.chip = obj["Chip"].toInt();
  n.channel = obj["Channel"].toInt();
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::PWM& n)
{
  m_stream << n.chip << n.channel << n.polarity;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::PWM& n)
{
  m_stream >> n.chip >> n.channel >> n.polarity;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::PWM& n)
{
  stream.StartObject();
  obj["Chip"] = n.chip;
  obj["Channel"] = n.channel;
  obj["Polarity"] = n.polarity;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::PWM& n)
{
  if(!obj.tryGet("Chip"))
    return;
  n.chip = obj["Chip"].toInt();
  n.channel = obj["Channel"].toInt();
  n.polarity = obj["Polarity"].toInt();
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::HID& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::HID& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::HID& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::HID& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::Custom& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::Custom& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::Custom& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::Custom& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::Port& n)
{
  m_stream << n.name << n.path << n.control;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::Port& n)
{
  m_stream >> n.name >> n.path >> n.control;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::Port& n)
{
  stream.StartObject();
  obj["Name"] = n.name;
  obj["Path"] = n.path;
  obj["Control"] = n.control;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::Port& n)
{
  n.name <<= obj["Name"];
  n.path <<= obj["Path"];
  n.control <<= obj["Control"];
}

template <>
void DataStreamReader::read(const Protocols::SimpleIOSpecificSettings& n)
{
  m_stream << n.ports << n.board << n.osc_configuration;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIOSpecificSettings& n)
{
  m_stream >> n.ports >> n.board >> n.osc_configuration;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIOSpecificSettings& n)
{
  obj["Ports"] = n.ports;
  obj["Board"] = n.board;
  if(n.osc_configuration)
    obj["OSC"] = *n.osc_configuration;
}

template <>
void JSONWriter::write(Protocols::SimpleIOSpecificSettings& n)
{
  n.ports <<= obj["Ports"];
  if(auto board = obj.tryGet("Board"))
    n.board = board->toString();
  if(auto conf = obj.tryGet("OSC"))
    n.osc_configuration = conf.value().to<ossia::net::osc_protocol_configuration>();
}
#endif
