#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIOSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

JSON_METADATA(Protocols::SimpleIO::GPIO, "GPIO")
JSON_METADATA(Protocols::SimpleIO::PWM, "PWM")
JSON_METADATA(Protocols::SimpleIO::ADC, "ADC")
JSON_METADATA(Protocols::SimpleIO::DAC, "DAC")

template <>
void DataStreamReader::read(const Protocols::SimpleIO::GPIO& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::GPIO& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::GPIO& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::GPIO& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::ADC& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::ADC& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::ADC& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::ADC& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::DAC& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::DAC& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::DAC& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::DAC& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::PWM& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::PWM& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIO::PWM& n)
{
  stream.StartObject();
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::SimpleIO::PWM& n)
{
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::Port& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::Port& n)
{
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
  m_stream << n.ports;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIOSpecificSettings& n)
{
  m_stream >> n.ports;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SimpleIOSpecificSettings& n)
{
  obj["Ports"] = n.ports;
}

template <>
void JSONWriter::write(Protocols::SimpleIOSpecificSettings& n)
{
  n.ports <<= obj["Ports"];
}
#endif
