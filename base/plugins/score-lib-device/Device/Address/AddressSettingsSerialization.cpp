// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include "AddressSettings.hpp"
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <Device/Address/ClipMode.hpp>
#include <State/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Value.hpp>
#include <score/serialization/AnySerialization.hpp>
#include <State/ValueSerialization.hpp>


template <>
void DataStreamReader::read(
    const Device::AddressSettingsCommon& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode << n.unit
           << n.repetitionFilter << n.extendedAttributes;
}

template <>
void DataStreamWriter::write(Device::AddressSettingsCommon& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode >> n.unit
           >> n.repetitionFilter >> n.extendedAttributes;
}

template <>
void JSONObjectReader::read(
    const Device::AddressSettingsCommon& n)
{
  // Metadata
  if(n.ioType)
    obj[strings.ioType] = Device::AccessModeText()[*n.ioType];
  obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];
  obj[strings.Unit]
      = QString::fromStdString(ossia::get_pretty_unit_text(n.unit));

  obj[strings.RepetitionFilter] = static_cast<bool>(n.repetitionFilter);

  // Value, domain and type
  readFrom(n.value);
  obj[strings.Domain] = toJsonObject(n.domain);

  obj[strings.Extended] = toJsonObject(n.extendedAttributes);
}

template <>
void JSONObjectWriter::write(Device::AddressSettingsCommon& n)
{
  n.ioType = Device::AccessModeText().key(obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(obj[strings.ClipMode].toString());
  n.unit
      = ossia::parse_pretty_unit(obj[strings.Unit].toString().toStdString());

  n.repetitionFilter = (ossia::repetition_filter) obj[strings.RepetitionFilter].toBool();

  writeTo(n.value);
  n.domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());

  n.extendedAttributes = fromJsonObject<score::any_map>(obj[strings.Extended]);
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamReader::read(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.name;

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamWriter::write(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.name;

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectReader::read(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  obj[strings.Name] = n.name;
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectWriter::write(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.name = obj[strings.Name].toString();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamReader::read(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.address;

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamWriter::write(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.address;

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectReader::read(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  obj[strings.Address] = toJsonObject(n.address);
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectWriter::write(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.address = fromJsonObject<State::Address>(obj[strings.Address]);
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(
    const Device::FullAddressAccessorSettings& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode
           << n.repetitionFilter << n.extendedAttributes << n.address;
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamWriter::write(Device::FullAddressAccessorSettings& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode
      >> n.repetitionFilter >> n.extendedAttributes >> n.address;
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONObjectReader::read(
    const Device::FullAddressAccessorSettings& n)
{
  // Metadata
  if(n.ioType)
    obj[strings.ioType] = Device::AccessModeText()[*n.ioType];
  obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];

  obj[strings.RepetitionFilter] = static_cast<bool>(n.repetitionFilter);

  // Value, domain and type
  readFrom(n.value);
  obj[strings.Domain] = toJsonObject(n.domain);
  obj[strings.Extended] = toJsonObject(n.extendedAttributes);

  obj[strings.Address] = toJsonObject(n.address);
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectWriter::write(Device::FullAddressAccessorSettings& n)
{
  n.ioType = Device::AccessModeText().key(obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(obj[strings.ClipMode].toString());

  n.repetitionFilter = (ossia::repetition_filter)obj[strings.RepetitionFilter].toBool();


  writeTo(n.value);

  n.domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());
  n.extendedAttributes = fromJsonObject<score::any_map>(obj[strings.Extended]);


  n.address = fromJsonObject<State::AddressAccessor>(obj[strings.Address]);
}

