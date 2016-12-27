#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <QDataStream>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

#include "Address.hpp"

/// AccessorVector ///
template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::AccessorVector& a)
{
  int32_t n = a.size();
  m_stream << n;
  for (int32_t i = 0; i < n; i++)
    m_stream << (int32_t)a[i];

  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::AccessorVector& a)
{
  int32_t size;
  m_stream >> size;
  for (int i = 0; i < size; i++)
  {
    int32_t t;
    m_stream >> t;
    a.push_back(t);
  }

  checkDelimiter();
}

/// Address ///
template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Address& a)
{
  m_stream << a.device << a.path;
  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::Address& a)
{
  obj[strings.Device] = a.device;
  obj[strings.Path] = a.path.join('/');
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::Address& a)
{
  m_stream >> a.device >> a.path;
  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::Address& a)
{
  a.device = obj[strings.Device].toString();

  auto path = obj[strings.Path].toString();

  if (!path.isEmpty())
    a.path = obj[strings.Path].toString().split('/');
}

/// AddressQualifiers ///
template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::destination_qualifiers& a)
{
  m_stream << a.accessors << a.unit;
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const ossia::destination_qualifiers& a)
{
  obj["Accessors"] = toJsonValueArray(a.accessors);
  obj[strings.Unit]
      = QString::fromStdString(ossia::get_pretty_unit_text(a.unit));
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(ossia::destination_qualifiers& a)
{
  m_stream >> a.accessors >> a.unit;
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(ossia::destination_qualifiers& a)
{
  auto arr = obj["Accessors"].toArray();
  for (auto v : arr)
  {
    a.accessors.push_back(v.toInt());
  }
  a.unit
      = ossia::parse_pretty_unit(obj[strings.Unit].toString().toStdString());
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::DestinationQualifiers& a)
{
  readFrom(a.get());
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::DestinationQualifiers& a)
{
  readFrom(a.get());
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::DestinationQualifiers& a)
{
  writeTo(a.get());
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::DestinationQualifiers& a)
{
  writeTo(a.get());
}

/// AddressAccessor ///
template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::AddressAccessor& rel)
{
  m_stream << rel.address << rel.qualifiers;

  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::AddressAccessor& rel)
{
  obj[strings.address] = toJsonObject(rel.address);
  readFrom(rel.qualifiers);
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::AddressAccessor& rel)
{
  m_stream >> rel.address >> rel.qualifiers;

  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::AddressAccessor& rel)
{
  fromJsonObject(obj[strings.address], rel.address);
  writeTo(rel.qualifiers);
}

/// AddressAccessorHead ///
template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::AddressAccessorHead& rel)
{
  m_stream << rel.name << rel.qualifiers;

  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::AddressAccessorHead& rel)
{
  obj[strings.Name] = rel.name;
  readFrom(rel.qualifiers);
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::AddressAccessorHead& rel)
{
  m_stream >> rel.name >> rel.qualifiers;

  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::AddressAccessorHead& rel)
{
  rel.name = obj[strings.Name].toString();
  writeTo(rel.qualifiers);
}
