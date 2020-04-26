// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Address.hpp"

#include <State/Unit.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>


/// Address ///
template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::Address& a)
{
  m_stream << a.device << a.path;
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::Address& a)
{
  const auto& str = a.toString().toUtf8();
  stream.String(str.data(), str.size());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::Address& a)
{
  m_stream >> a.device >> a.path;
  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::Address& a)
{
  auto addr = State::parseAddress(QString::fromUtf8(base.GetString(), base.GetStringLength()));
  if(addr)
    a = *std::move(addr);
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
JSONReader::read(const ossia::destination_qualifiers& a)
{
  obj[strings.Accessors] = a.accessors;
  obj[strings.Unit] = State::prettyUnitText(a.unit);
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::destination_qualifiers& a)
{
  m_stream >> a.accessors >> a.unit;
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONWriter::write(ossia::destination_qualifiers& a)
{
  a.accessors <<= obj[strings.Accessors];
  a.unit
      = ossia::parse_pretty_unit(obj[strings.Unit].toStdString());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::DestinationQualifiers& a)
{
  m_stream << a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONReader::read(const State::DestinationQualifiers& a)
{
  read(a.get());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::DestinationQualifiers& a)
{
  m_stream >> a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONWriter::write(State::DestinationQualifiers& a)
{
  write(a.get());
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
JSONReader::read(const State::AddressAccessor& rel)
{
  const auto& str = rel.toString().toUtf8();
  stream.String(str.data(), str.size());
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
JSONWriter::write(State::AddressAccessor& rel)
{
  auto addr = State::parseAddressAccessor(QString::fromUtf8(base.GetString(), base.GetStringLength()));
  if(addr)
    rel = *std::move(addr);
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
JSONReader::read(const State::AddressAccessorHead& rel)
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
JSONWriter::write(State::AddressAccessorHead& rel)
{
  rel.name = obj[strings.Name].toString();
  writeTo(rel.qualifiers);
}
