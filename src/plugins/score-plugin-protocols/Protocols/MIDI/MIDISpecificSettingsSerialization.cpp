// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDISpecificSettings.hpp"

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

JSON_METADATA(libremidi::usb_device_identifier, "usb_device_identifier")
JSON_METADATA(libremidi::uuid, "uuid")

template <>
void DataStreamReader::read(const ossia::monostate& n)
{
}
template <>
void DataStreamWriter::write(ossia::monostate& n)
{
}
template <>
void JSONReader::read(const ossia::monostate& n)
{
  stream.Null();
}
template <>
void JSONWriter::write(ossia::monostate& n)
{
}

template <>
void DataStreamReader::read(const uint64_t& n)
{
  m_stream.stream << n;
}
template <>
void DataStreamWriter::write(uint64_t& n)
{
  m_stream.stream >> n;
}
template <>
void JSONReader::read(const uint64_t& n)
{
  stream.Uint64(n);
}
template <>
void JSONWriter::write(uint64_t& n)
{
  n = base.GetUint64();
}

template <>
void DataStreamReader::read(const libremidi::uuid& n)
{
  m_stream.stream.writeRawData((const char*)n.bytes.data(), n.bytes.size());
}
template <>
void DataStreamWriter::write(libremidi::uuid& n)
{
  m_stream.stream.readRawData((char*)n.bytes.data(), n.bytes.size());
}

template <>
void DataStreamReader::read(const libremidi::usb_device_identifier& n)
{
  m_stream << n.vendor_id << n.product_id;
}
template <>
void DataStreamWriter::write(libremidi::usb_device_identifier& n)
{
  m_stream >> n.vendor_id >> n.product_id;
}

template <>
void JSONReader::read(const libremidi::uuid& n)
{
  auto uid = score::uuids::toByteArray(
      *reinterpret_cast<const score::uuids::uuid*>(n.bytes.data()));
  stream.String(uid.data(), uid.size());
}
template <>
void JSONWriter::write(libremidi::uuid& n)
{
  if(base.GetStringLength() >= 36)
  {
    auto uid = score::uuids::string_generator::compute(
        base.GetString(), base.GetString() + base.GetStringLength());
    memcpy(n.bytes.data(), uid.data, n.bytes.size());
  }
}

template <>
void JSONReader::read(const libremidi::usb_device_identifier& n)
{
  stream.StartArray();
  stream.Int(n.vendor_id);
  stream.Int(n.product_id);
  stream.EndArray();
}
template <>
void JSONWriter::write(libremidi::usb_device_identifier& n)
{
  const auto& arr = base.GetArray();
  n.vendor_id = arr[0].GetInt();
  n.product_id = arr[1].GetInt();
}

template <>
void DataStreamReader::read(const Protocols::MIDISpecificSettings& n)
{
  m_stream << n.io << n.handle.api << n.createWholeTree << n.virtualPort
           << n.handle.container << n.handle.device << n.handle.port
           << n.handle.manufacturer << n.handle.product << n.handle.serial
           << n.handle.device_name << n.handle.port_name << n.handle.display_name
           << n.velocityZeroIsNoteOff;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::MIDISpecificSettings& n)
{
  m_stream >> n.io >> n.handle.api >> n.createWholeTree >> n.virtualPort
      >> n.handle.container >> n.handle.device >> n.handle.port >> n.handle.manufacturer
      >> n.handle.product >> n.handle.serial >> n.handle.device_name
      >> n.handle.port_name >> n.handle.display_name >> n.velocityZeroIsNoteOff;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::MIDISpecificSettings& n)
{
  obj["API"] = n.handle.api;
  obj["IO"] = n.io;

  if(!get_if<libremidi_variant_alias::monostate>(&n.handle.container))
    obj["Container"] = n.handle.container;
  if(!get_if<libremidi_variant_alias::monostate>(&n.handle.device))
    obj["DeviceIdentifier"] = n.handle.device;

  obj["Port"] = n.handle.port;

  if(!n.handle.manufacturer.empty())
    obj["Manufacturer"] = n.handle.manufacturer;
  if(!n.handle.product.empty())
    obj["Product"] = n.handle.product;
  if(!n.handle.serial.empty())
    obj["Serial"] = n.handle.serial;
  if(!n.handle.device_name.empty())
    obj["DeviceName"] = n.handle.device_name;
  if(!n.handle.port_name.empty())
    obj["PortName"] = n.handle.port_name;
  if(!n.handle.display_name.empty())
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
    if(auto v = obj.tryGet("Container"))
      n.handle.container <<= *v;
    if(auto v = obj.tryGet("DeviceIdentifier"))
      n.handle.device <<= *v;
    if(auto v = obj.tryGet("Manufacturer"))
      n.handle.manufacturer = v->toStdString();
    if(auto v = obj.tryGet("Product"))
      n.handle.product = v->toStdString();
    if(auto v = obj.tryGet("Serial"))
      n.handle.serial = v->toStdString();
    if(auto v = obj.tryGet("DeviceName"))
      n.handle.device_name = v->toStdString();
    if(auto v = obj.tryGet("PortName"))
      n.handle.port_name = v->toStdString();
    if(auto v = obj.tryGet("DisplayName"))
      n.handle.display_name = v->toStdString();
  }

  if(auto it = obj.tryGet("CreateWholeTree"))
    n.createWholeTree = it->toBool();
  else
    n.createWholeTree = true;

  if(auto it = obj.tryGet("API"))
    n.handle.api = (libremidi::API)it->toInt();
  else
    n.handle.api = libremidi::midi1::default_api();

  if(auto it = obj.tryGet("VirtualPort"))
    n.virtualPort = it->toBool();
  else
    n.virtualPort = false;

  if(auto it = obj.tryGet("VelocityZeroIsNoteOff"))
    n.velocityZeroIsNoteOff = it->toBool();
  else
    n.velocityZeroIsNoteOff = false;
}
