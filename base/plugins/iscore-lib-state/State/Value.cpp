#include "Value.hpp"
#include "Unit.hpp"
#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/detail/apply.hpp>
#include <State/ValueConversion.hpp>

namespace State
{
bool Value::operator==(const Value& m) const
{
  return val == m.val;
}

bool Value::operator!=(const Value& m) const
{
  return val != m.val;
}

ValueImpl::ValueImpl(impulse v) : m_variant{v}
{
}
ValueImpl::ValueImpl(int v) : m_variant{v}
{
}
ValueImpl::ValueImpl(float v) : m_variant{v}
{
}
ValueImpl::ValueImpl(double v) : m_variant{(float)v}
{
}
ValueImpl::ValueImpl(bool v) : m_variant{v}
{
}
ValueImpl::ValueImpl(std::string v) : m_variant{std::move(v)}
{
}

ValueImpl::ValueImpl(char v) : m_variant{v}
{
}
ValueImpl::ValueImpl(vec2f v) : m_variant{v}
{
}
ValueImpl::ValueImpl(vec3f v) : m_variant{v}
{
}
ValueImpl::ValueImpl(vec4f v) : m_variant{v}
{
}
ValueImpl::ValueImpl(tuple_t v) : m_variant{std::move(v)}
{
}

ValueImpl& ValueImpl::operator=(impulse v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(int v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(float v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(double v)
{
  m_variant = (float)v;
  return *this;
}
ValueImpl& ValueImpl::operator=(bool v)
{
  m_variant = v;
  return *this;
}

ValueImpl& ValueImpl::operator=(const std::string& v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(std::string&& v)
{
  m_variant = std::move(v);
  return *this;
}

ValueImpl& ValueImpl::operator=(char v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(vec2f v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(vec3f v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(vec4f v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(const tuple_t& v)
{
  m_variant = v;
  return *this;
}
ValueImpl& ValueImpl::operator=(tuple_t&& v)
{
  m_variant = std::move(v);
  return *this;
}

bool ValueImpl::operator==(const ValueImpl& other) const
{
  return m_variant == other.m_variant;
}

bool ValueImpl::operator!=(const ValueImpl& other) const
{
  return m_variant != other.m_variant;
}

bool ValueImpl::isNumeric() const
{
  const auto t = which();
  return t == ValueType::Float || t == ValueType::Int;
}

bool ValueImpl::isValid() const
{
  return m_variant.which() != m_variant.npos;
}

bool ValueImpl::isArray() const
{
  const auto t = which();

  switch(t)
  {
    case State::ValueType::Vec2f:
    case State::ValueType::Vec3f:
    case State::ValueType::Vec4f:
    case State::ValueType::Tuple:
      return true;
    default:
      return false;
  }
}

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m)
{
  s << convert::textualType(m) << convert::toPrettyString(m);
  return s;
}

ossia::value toOSSIAValue(const State::ValueImpl& val)
{
  struct
  {
    using return_type = ossia::value;
    return_type operator()() const
    {
      return ossia::value{};
    }
    return_type operator()(const State::impulse&) const
    {
      return ossia::impulse{};
    }
    return_type operator()(int v) const
    {
      return v;
    }
    return_type operator()(float v) const
    {
      return v;
    }
    return_type operator()(bool v) const
    {
      return v;
    }
    return_type operator()(const QString& v) const
    {
      return v.toStdString();
    }
    return_type operator()(QChar v) const
    {
      return v.toLatin1();
    }
    return_type operator()(const std::string& v) const
    {
      return v;
    }
    return_type operator()(char v) const
    {
      return v;
    }
    return_type operator()(const State::vec2f& v) const
    {
      return v;
    }
    return_type operator()(const State::vec3f& v) const
    {
      return v;
    }
    return_type operator()(const State::vec4f& v) const
    {
      return v;
    }
    return_type operator()(const State::tuple_t& v) const
    {
      std::vector<ossia::value> ossia_tuple;
      ossia_tuple.reserve(v.size());
      for (const auto& tuple_elt : v)
      {
        ossia_tuple.push_back(eggs::variants::apply(*this, tuple_elt.impl()));
      }

      return ossia_tuple;
    }
  } visitor{};

  return ossia::apply(visitor, val.impl());
}

Value fromOSSIAValue(const ossia::value& val)
{
  struct
  {
    using return_type = State::Value;
    return_type operator()(ossia::impulse) const
    {
      return State::Value::fromValue(State::impulse{});
    }
    return_type operator()(int32_t v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(float v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(bool v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(char v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(const std::string& v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(ossia::vec2f v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(ossia::vec3f v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(ossia::vec4f v) const
    {
      return State::Value::fromValue(v);
    }
    return_type operator()(const std::vector<ossia::value>& v) const
    {
      State::tuple_t tuple;

      tuple.reserve(v.size());
      for (const auto& e : v)
      {
        tuple.push_back(fromOSSIAValue(e).val); // TODO REVIEW THIS
      }

      return State::Value::fromValue(std::move(tuple));
    }
  } visitor{};

  if (val.valid())
    return ossia::apply_nonnull(visitor, val.v);
  return {};
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
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var.get());
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::Unit& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var.get());
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::readFrom(*this, var);
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::unit_t& var)
{
  TSerializer<DataStream, ossia::unit_t>::writeTo(*this, var);
}
