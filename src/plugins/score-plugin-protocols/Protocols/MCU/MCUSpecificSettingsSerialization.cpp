// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <libremidi/backends.hpp>

/**
 * The backend and the identifying strings, alongside the port handle, which
 * does not survive a save: it is an ALSA sequencer client number or a CoreMIDI
 * unique id and changes when the interface is replugged.
 * optimistic_serialized_port_lookup() re-finds a port from the rest, but every
 * one of its heuristics requires the target to name an API.
 *
 * `container` and `device` are left out: they are variants whose serializers
 * live in the MIDI device's translation unit, and the lookup only uses them to
 * break a tie between candidates already found by name.
 */
template <>
void DataStreamReader::read(const libremidi::port_information& n)
{
  m_stream << n.port << n.manufacturer << n.device_name << n.port_name << n.display_name
           << n.api << n.product << n.serial;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(libremidi::port_information& n)
{
  m_stream >> n.port >> n.manufacturer >> n.device_name >> n.port_name >> n.display_name
      >> n.api >> n.product >> n.serial;
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
  obj["API"] = n.api;
  if(!n.product.empty())
    obj["Product"] = n.product;
  if(!n.serial.empty())
    obj["Serial"] = n.serial;
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

  // A score saved without an API gets the machine's default, which is the one
  // the settings widget lists ports from.
  if(auto v = obj.tryGet("API"))
    n.api = (libremidi::API)v->toInt();
  else
    n.api = libremidi::midi1::default_api();
  if(auto v = obj.tryGet("Product"))
    n.product = v->toStdString();
  if(auto v = obj.tryGet("Serial"))
    n.serial = v->toStdString();
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
  m_stream << n.input_handle << n.output_handle << n.api << n.mode << n.map
           << n.channel;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::MCUSpecificSettings& n)
{
  m_stream >> n.input_handle >> n.output_handle >> n.api >> n.mode >> n.map
      >> n.channel;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::MCUSpecificSettings& n)
{
  obj["API"] = n.api;
  obj["Input"] = n.input_handle;
  obj["Output"] = n.output_handle;
  obj["Mode"] = (int)n.mode;

  // Only for the mode it belongs to: an empty map name would otherwise read as
  // a map whose package is missing.
  if(n.mode == Protocols::MCUSpecificSettings::MidiDeviceMap)
  {
    obj["Map"] = n.map;
    obj["Channel"] = n.channel;
  }
}

template <>
void JSONWriter::write(Protocols::MCUSpecificSettings& n)
{
  n.api <<= obj["API"];
  n.input_handle <<= obj["Input"];
  n.output_handle <<= obj["Output"];

  // A score with none of these keys is a Mackie Control device.
  if(auto mode = obj.tryGet("Mode"))
    n.mode = static_cast<Protocols::MCUSpecificSettings::Mode>(mode->toInt());
  if(auto m = obj.tryGet("Map"))
    n.map = m->toString();
  if(auto c = obj.tryGet("Channel"))
    n.channel = c->toInt();
}
