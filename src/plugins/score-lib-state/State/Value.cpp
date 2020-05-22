// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Value.hpp"

#include "Unit.hpp"
#include "ValueParser.hpp"

#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/apply.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/value.hpp>

#include <QDebug>

#include <wobjectimpl.h>
// W_GADGET_IMPL(State::Unit)

namespace State
{
SCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m)
{
  s << convert::textualType(m) << convert::toPrettyString(m);
  return s;
}

Unit::Unit() noexcept : unit{std::make_unique<ossia::unit_t>()} { }

Unit::Unit(const Unit& other) noexcept : unit{std::make_unique<ossia::unit_t>(*other.unit)} { }

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

Unit::~Unit() { }

Unit::Unit(const ossia::unit_t& other) noexcept : unit{std::make_unique<ossia::unit_t>(other)} { }

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

Unit::operator const ossia::unit_t &() const noexcept
{
  return *unit;
}

Unit::operator ossia::unit_t &() noexcept
{
  return *unit;
}

QLatin1String prettyUnitText(const ossia::unit_t& u)
{
  auto unit = ossia::get_pretty_unit_text(u);
  return QLatin1String(unit.data(), unit.size());
}

std::optional<ossia::value> parseValue(const std::string& input)
{
  auto f(std::begin(input)), l(std::end(input));
  Value_parser<decltype(f)> p;
  try
  {
    ossia::value result;
    bool ok = qi::phrase_parse(f, l, p, qi::standard::space, result);

    if (!ok)
    {
      return {};
    }

    return result;
  }
  catch (const qi::expectation_failure<decltype(f)>& e)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
  catch (...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}
}
