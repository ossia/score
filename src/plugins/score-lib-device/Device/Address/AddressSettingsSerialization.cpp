// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressSettings.hpp"

#include <Device/Address/ClipMode.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Domain.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

template <>
void DataStreamReader::read(const Device::AddressSettingsCommon& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode << n.unit << n.repetitionFilter
           << n.extendedAttributes;
}

template <>
void DataStreamWriter::write(Device::AddressSettingsCommon& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode >> n.unit >> n.repetitionFilter
      >> n.extendedAttributes;
}

template <>
void JSONReader::read(const Device::AddressSettingsCommon& n)
{
  // Metadata
  if (n.ioType)
    obj[strings.ioType] = Device::AccessModeText()[*n.ioType];

  obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];

  if(n.unit.get())
    obj[strings.Unit] = State::prettyUnitText(n.unit);

  obj[strings.RepetitionFilter] = static_cast<bool>(n.repetitionFilter);

  // Value, domain and type
  obj[strings.Value] = n.value;
  obj[strings.Domain] = n.domain;

  if(!n.extendedAttributes.empty())
    obj[strings.Extended] = n.extendedAttributes;
}

template <>
void JSONWriter::write(Device::AddressSettingsCommon& n)
{
  if(auto iot = obj.tryGet(strings.ioType))
    n.ioType = Device::AccessModeText().key(iot->toString());
  n.clipMode = Device::ClipModeStringMap().key(obj[strings.ClipMode].toString());

  if(auto unit = obj.tryGet(strings.Unit))
    n.unit = ossia::parse_pretty_unit(unit->toString().toStdString());

  n.repetitionFilter = (ossia::repetition_filter)obj[strings.RepetitionFilter].toBool();

  n.value <<= obj[strings.Value];
  n.domain <<= obj[strings.Domain];

  if(auto ext = obj.tryGet(strings.Extended))
    n.extendedAttributes <<= *ext;
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.name;

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.name;

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::AddressSettings& n)
{
  stream.StartObject();
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  obj[strings.Name] = n.name;
  stream.EndObject();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.name = obj[strings.Name].toString();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.address;

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.address;

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::FullAddressSettings& n)
{
  stream.StartObject();
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  obj[strings.Address] = n.address;
  stream.EndObject();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.address <<= obj[strings.Address];
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::FullAddressAccessorSettings& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode << n.repetitionFilter
           << n.extendedAttributes << n.address;
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::FullAddressAccessorSettings& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode >> n.repetitionFilter
      >> n.extendedAttributes >> n.address;
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::FullAddressAccessorSettings& n)
{
  stream.StartObject();
  // Metadata
  if (n.ioType)
    obj[strings.ioType] = Device::AccessModeText()[*n.ioType];
  obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];

  obj[strings.RepetitionFilter] = static_cast<bool>(n.repetitionFilter);

  // Value, domain and type
  obj[strings.Value] = n.value;
  obj[strings.Domain] = n.domain;
  obj[strings.Extended] = n.extendedAttributes;

  obj[strings.Address] = n.address;
  stream.EndObject();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::FullAddressAccessorSettings& n)
{
  n.ioType = Device::AccessModeText().key(obj[strings.ioType].toString());
  n.clipMode = Device::ClipModeStringMap().key(obj[strings.ClipMode].toString());

  n.repetitionFilter = (ossia::repetition_filter)obj[strings.RepetitionFilter].toBool();

  n.value <<= obj[strings.Value];
  n.domain <<= obj[strings.Domain];
  n.extendedAttributes <<= obj[strings.Extended];
  n.address <<= obj[strings.Address];
}
