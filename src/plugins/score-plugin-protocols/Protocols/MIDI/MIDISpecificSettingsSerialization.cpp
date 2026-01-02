// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDISpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::MIDISpecificSettings& n)
{
  m_stream << n.io << n.api << n.createWholeTree << n.virtualPort << n.handle.port
           << n.handle.manufacturer << n.handle.device_name << n.handle.port_name
           << n.handle.display_name << n.velocityZeroIsNoteOff;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::MIDISpecificSettings& n)
{
  m_stream >> n.io >> n.api >> n.createWholeTree >> n.virtualPort >> n.handle.port
      >> n.handle.manufacturer >> n.handle.device_name >> n.handle.port_name
      >> n.handle.display_name >> n.velocityZeroIsNoteOff;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::MIDISpecificSettings& n)
{
  obj["API"] = n.api;
  obj["IO"] = n.io;

  obj["Port"] = n.handle.port;
  obj["Manufacturer"] = n.handle.manufacturer;
  obj["DeviceName"] = n.handle.device_name;
  obj["PortName"] = n.handle.port_name;
  obj["DisplayName"] = n.handle.display_name;

  obj["CreateWholeTree"] = n.createWholeTree;
  obj["VirtualPort"] = n.virtualPort;
  obj["VelocityZeroIsNoteOff"] = n.velocityZeroIsNoteOff;
}

template <>
void JSONWriter::write(Protocols::MIDISpecificSettings& n)
{
  n.io <<= obj["IO"];

  if(auto ep = obj.tryGet("Endpoint"))
  {
    // Old save format
    n.handle.port_name = ep->toStdString();
    n.handle.display_name = ep->toStdString();
  }
  else
  {
    n.handle.port = obj["Port"].toUInt64();
    n.handle.manufacturer = obj["Manufacturer"].toStdString();
    n.handle.device_name = obj["DeviceName"].toStdString();
    n.handle.port_name = obj["PortName"].toStdString();
    n.handle.display_name = obj["DisplayName"].toStdString();
  }

  if(auto it = obj.tryGet("CreateWholeTree"))
    n.createWholeTree = it->toBool();
  else
    n.createWholeTree = true;

  if(auto it = obj.tryGet("API"))
    n.api = (libremidi::API)it->toInt();
  else
    n.api = libremidi::midi1::default_api();

  if(auto it = obj.tryGet("VirtualPort"))
    n.virtualPort = it->toBool();
  else
    n.virtualPort = false;

  if(auto it = obj.tryGet("VelocityZeroIsNoteOff"))
    n.velocityZeroIsNoteOff = it->toBool();
  else
    n.velocityZeroIsNoteOff = false;
}
