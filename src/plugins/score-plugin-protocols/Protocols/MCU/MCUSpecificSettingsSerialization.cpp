// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const libremidi::port_information& n)
{
  m_stream << n.port << n.manufacturer << n.device_name << n.port_name << n.display_name;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(libremidi::port_information& n)
{
  m_stream >> n.port >> n.manufacturer >> n.device_name >> n.port_name >> n.display_name;
  checkDelimiter();
}

template <>
void JSONReader::read(const libremidi::port_information& n)
{
  stream.StartObject();
  obj["Port"] = n.port;
  obj["Manufacturer"] = n.manufacturer;
  obj["DeviceName"] = n.device_name;
  obj["PortName"] = n.port_name;
  obj["DisplayName"] = n.display_name;
  stream.EndObject();
}

template <>
void JSONWriter::write(libremidi::port_information& n)
{
  n.port = obj["Port"].toUInt64();
  n.manufacturer = obj["Manufacturer"].toStdString();
  n.device_name = obj["DeviceName"].toStdString();
  n.port_name = obj["PortName"].toStdString();
  n.display_name = obj["DisplayName"].toStdString();
}

template <>
void DataStreamReader::read(const libremidi::input_port& n)
{
  read(static_cast<const libremidi::port_information&>(n));
}

template <>
void DataStreamWriter::write(libremidi::input_port& n)
{
  write(static_cast<libremidi::port_information&>(n));
}

template <>
void JSONReader::read(const libremidi::input_port& n)
{
  read(static_cast<const libremidi::port_information&>(n));
}

template <>
void JSONWriter::write(libremidi::input_port& n)
{
  write(static_cast<libremidi::port_information&>(n));
}

template <>
void DataStreamReader::read(const libremidi::output_port& n)
{
  read(static_cast<const libremidi::port_information&>(n));
}

template <>
void DataStreamWriter::write(libremidi::output_port& n)
{
  write(static_cast<libremidi::port_information&>(n));
}

template <>
void JSONReader::read(const libremidi::output_port& n)
{
  read(static_cast<const libremidi::port_information&>(n));
}

template <>
void JSONWriter::write(libremidi::output_port& n)
{
  write(static_cast<libremidi::port_information&>(n));
}

template <>
void DataStreamReader::read(const Protocols::MCUSpecificSettings& n)
{
  m_stream << n.input_handle << n.output_handle << n.api << n.mode;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::MCUSpecificSettings& n)
{
  m_stream >> n.input_handle >> n.output_handle >> n.api >> n.mode;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::MCUSpecificSettings& n)
{
  obj["API"] = n.api;
  obj["Input"] = n.input_handle;
  obj["Output"] = n.output_handle;
  // FIXME mode
}

template <>
void JSONWriter::write(Protocols::MCUSpecificSettings& n)
{
  n.api <<= obj["API"];
  n.input_handle <<= obj["Input"];
  n.output_handle <<= obj["Output"];
  // FIXME mode
}
