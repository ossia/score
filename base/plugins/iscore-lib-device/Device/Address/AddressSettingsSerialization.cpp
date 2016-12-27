#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "AddressSettings.hpp"
#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Value.hpp>
#include <iscore/serialization/AnySerialization.hpp>


template <>
void Visitor<Reader<DataStream>>::read(
    const Device::AddressSettingsCommon& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode << n.unit
           << n.repetitionFilter << n.extendedAttributes;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Device::AddressSettingsCommon& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode >> n.unit
           >> n.repetitionFilter >> n.extendedAttributes;
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(
    const Device::AddressSettingsCommon& n)
{
  // Metadata
  m_obj[strings.ioType] = Device::IOTypeStringMap()[n.ioType];
  m_obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];
  m_obj[strings.Unit]
      = QString::fromStdString(ossia::get_pretty_unit_text(n.unit));

  m_obj[strings.RepetitionFilter] = n.repetitionFilter;

  // Value, domain and type
  readFrom(n.value);
  m_obj[strings.Domain] = toJsonObject(n.domain);

  m_obj[strings.Extended] = toJsonObject(n.extendedAttributes);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Device::AddressSettingsCommon& n)
{
  n.ioType = Device::IOTypeStringMap().key(m_obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(m_obj[strings.ClipMode].toString());
  n.unit
      = ossia::parse_pretty_unit(m_obj[strings.Unit].toString().toStdString());

  n.repetitionFilter = m_obj[strings.RepetitionFilter].toBool();

  writeTo(n.value);
  n.domain = fromJsonObject<Device::Domain>(m_obj[strings.Domain].toObject());

  n.extendedAttributes = fromJsonObject<iscore::any_map>(m_obj[strings.Extended]);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<DataStream>>::read(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.name;

  insertDelimiter();
}
template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.name;

  checkDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_obj[strings.Name] = n.name;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.name = m_obj[strings.Name].toString();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<DataStream>>::read(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.address;

  insertDelimiter();
}
template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.address;

  checkDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_obj[strings.Address] = toJsonObject(n.address);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.address = fromJsonObject<State::Address>(m_obj[strings.Address]);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::read(
    const Device::FullAddressAccessorSettings& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode
           << n.repetitionFilter << n.extendedAttributes << n.address;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::FullAddressAccessorSettings& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode
      >> n.repetitionFilter >> n.extendedAttributes >> n.address;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(
    const Device::FullAddressAccessorSettings& n)
{
  // Metadata
  m_obj[strings.ioType] = Device::IOTypeStringMap()[n.ioType];
  m_obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];

  m_obj[strings.RepetitionFilter] = n.repetitionFilter;

  // Value, domain and type
  readFrom(n.value);
  m_obj[strings.Domain] = toJsonObject(n.domain);
  m_obj[strings.Extended] = toJsonObject(n.extendedAttributes);

  m_obj[strings.Address] = toJsonObject(n.address);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::FullAddressAccessorSettings& n)
{
  n.ioType = Device::IOTypeStringMap().key(m_obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(m_obj[strings.ClipMode].toString());

  n.repetitionFilter = m_obj[strings.RepetitionFilter].toBool();


  writeTo(n.value);

  n.domain = fromJsonObject<Device::Domain>(m_obj[strings.Domain].toObject());
  n.extendedAttributes = fromJsonObject<iscore::any_map>(m_obj[strings.Extended]);


  n.address = fromJsonObject<State::AddressAccessor>(m_obj[strings.Address]);
}

