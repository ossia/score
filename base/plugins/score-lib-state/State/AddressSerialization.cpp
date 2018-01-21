// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <QDataStream>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>

#include "Address.hpp"


/// Address ///
template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Address& a)
{
  m_stream << a.device << a.path;
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::Address& a)
{
  obj[strings.Device] = a.device;
  obj[strings.Path] = a.path.join('/');
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::Address& a)
{
  m_stream >> a.device >> a.path;
  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::Address& a)
{
  a.device = obj[strings.Device].toString();

  auto path = obj[strings.Path].toString();

  if (!path.isEmpty())
    a.path = obj[strings.Path].toString().split('/');
}

/// AddressQualifiers ///
template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::destination_qualifiers& a)
{
  m_stream << a.accessors << a.unit;
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const ossia::destination_qualifiers& a)
{
  obj[strings.Accessors] = toJsonValueArray(a.accessors);
  obj[strings.Unit]
      = QString::fromStdString(ossia::get_pretty_unit_text(a.unit));
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::destination_qualifiers& a)
{
  m_stream >> a.accessors >> a.unit;
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(ossia::destination_qualifiers& a)
{
  auto arr = obj[strings.Accessors].toArray();
  for (auto v : arr)
  {
    a.accessors.push_back(v.toInt());
  }
  a.unit
      = ossia::parse_pretty_unit(obj[strings.Unit].toString().toStdString());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::DestinationQualifiers& a)
{
  m_stream << a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::DestinationQualifiers& a)
{
  readFrom(a.get());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::DestinationQualifiers& a)
{
  m_stream >> a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::DestinationQualifiers& a)
{
  writeTo(a.get());
}

/// AddressAccessor ///
template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::AddressAccessor& rel)
{
  m_stream << rel.address << rel.qualifiers;

  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::AddressAccessor& rel)
{
  obj[strings.address] = toJsonObject(rel.address);
  readFrom(rel.qualifiers);
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::AddressAccessor& rel)
{
  m_stream >> rel.address >> rel.qualifiers;

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::AddressAccessor& rel)
{
  fromJsonObject(obj[strings.address], rel.address);
  writeTo(rel.qualifiers);
}

/// AddressAccessorHead ///
template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::AddressAccessorHead& rel)
{
  m_stream << rel.name << rel.qualifiers;

  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::AddressAccessorHead& rel)
{
  obj[strings.Name] = rel.name;
  readFrom(rel.qualifiers);
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::AddressAccessorHead& rel)
{
  m_stream >> rel.name >> rel.qualifiers;

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::AddressAccessorHead& rel)
{
  rel.name = obj[strings.Name].toString();
  writeTo(rel.qualifiers);
}
