// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/Domain.hpp>
#include <State/DomainSerializationImpl.hpp>
#include <State/Unit.hpp>
#include <State/ValueSerializationImpl.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>

void TSerializer<DataStream, ossia::unit_t>::readFrom(
    DataStream::Serializer& s,
    const ossia::unit_t& var)
{
  s.stream() << (quint64)var.which();

  if (var)
  {
    ossia::apply_nonnull([&](auto unit) { s.stream() << (quint64)unit.which(); }, var.v);
  }

  s.insertDelimiter();
}

void TSerializer<DataStream, ossia::unit_t>::writeTo(
    DataStream::Deserializer& s,
    ossia::unit_t& var)
{
  quint64 ds_which;
  s.stream() >> ds_which;

  if (ds_which != (quint64)var.v.npos)
  {
    quint64 unit_which;
    s.stream() >> unit_which;
    var = ossia::make_unit(ds_which, unit_which);
  }
  s.checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::Domain& var)
{
  readFrom(var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::Domain& var)
{
  writeTo(var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::Domain& var)
{
  readFrom(var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::Domain& var)
{
  writeTo(var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}

/// Instance bounds ///

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::net::instance_bounds& n)
{
  m_stream << n.min_instances << n.max_instances;
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::net::instance_bounds& n)
{
  m_stream >> n.min_instances >> n.max_instances;
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const ossia::net::instance_bounds& b)
{
  obj[strings.Min] = b.min_instances;
  obj[strings.Max] = b.max_instances;
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(ossia::net::instance_bounds& n)
{
  n.min_instances = obj[strings.Min].toInt();
  n.max_instances = obj[strings.Max].toInt();
}
