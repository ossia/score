// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Value.hpp"
#include "Unit.hpp"
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/detail/apply.hpp>
#include <State/ValueConversion.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
namespace State
{
SCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m)
{
  s << convert::textualType(m) << convert::toPrettyString(m);
  return s;
}

Unit::Unit() noexcept : unit{std::make_unique<ossia::unit_t>()}
{
}

Unit::Unit(const Unit& other) noexcept
    : unit{std::make_unique<ossia::unit_t>(*other.unit)}
{
}

Unit::Unit(Unit&& other) noexcept : unit{std::move(other.unit)}
{
  other.unit = std::make_unique<ossia::unit_t>();
}

Unit& Unit::operator=(const Unit& other) noexcept
{
  *unit = *other.unit;
  return *this;
}

Unit& Unit::operator=(Unit&& other) noexcept
{
  *unit = std::move(*other.unit);
  return *this;
}

Unit::~Unit()
{
}

Unit::Unit(const ossia::unit_t& other) noexcept
    : unit{std::make_unique<ossia::unit_t>(other)}
{
}

Unit& Unit::operator=(const ossia::unit_t& other) noexcept
{
  *unit = other;
  return *this;
}

bool Unit::operator==(const Unit& other) const noexcept
{
  return *unit == *other.unit;
}

bool Unit::operator!=(const Unit& other) const noexcept
{
  return *unit != *other.unit;
}

const ossia::unit_t& Unit::get() const noexcept
{
  return *unit;
}

ossia::unit_t& Unit::get() noexcept
{
  return *unit;
}

Unit::operator const ossia::unit_t&() const noexcept
{
  return *unit;
}

Unit::operator ossia::unit_t&() noexcept
{
  return *unit;
}
}

void TSerializer<DataStream, ossia::unit_t>::readFrom(
    DataStream::Serializer& s, const ossia::unit_t& var)
{
  s.stream() << (quint64)var.which();

  if (var)
  {
    ossia::apply_nonnull(
        [&](auto unit) { s.stream() << (quint64)unit.which(); }, var.v);
  }

  s.insertDelimiter();
}

void TSerializer<DataStream, ossia::unit_t>::writeTo(
    DataStream::Deserializer& s, ossia::unit_t& var)
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
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var.get());
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var);
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var);
}
