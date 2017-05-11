#pragma once
#include <ossia/editor/value/impulse.hpp>
#include <QChar>
#include <QList>
#include <QString>
#include <algorithm>
#include <eggs/variant.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <ossia-qt/metatypes.hpp>
#include <iscore_lib_state_export.h>
#include <vector>

class DataStream;
class JSONObject;
class QDebug;

namespace State
{
using impulse = ossia::impulse;

class ValueImpl;
using vec2f = ossia::vec2f;
using vec3f = ossia::vec3f;
using vec4f = ossia::vec4f;
using tuple_t = std::vector<ValueImpl>;
enum class ValueType : std::size_t
{
  Float,
  Int,
  Vec2f,
  Vec3f,
  Vec4f,
  Impulse,
  Bool,
  String,
  Tuple,
  Char,
  NoValue = eggs::variant<>::npos
};

class ISCORE_LIB_STATE_EXPORT ValueImpl
{
  ISCORE_SERIALIZE_FRIENDS

public:
  using variant_t = eggs::
      variant<
        float,
        int,
        vec2f, vec3f, vec4f,
        impulse,
        bool,
        std::string,
        tuple_t, char>;

  ValueImpl() = default;
  ValueImpl(const ValueImpl&) = default;
  ValueImpl(ValueImpl&&) = default;

  ValueImpl& operator=(const ValueImpl&) = default;
  ValueImpl& operator=(ValueImpl&&) = default;

  ValueImpl(impulse v);
  ValueImpl(int v);
  ValueImpl(float v);
  ValueImpl(double v);
  ValueImpl(bool v);
  ValueImpl(std::string v);
  ValueImpl(char v);
  ValueImpl(vec2f v);
  ValueImpl(vec3f v);
  ValueImpl(vec4f v);
  ValueImpl(tuple_t v);

  ValueImpl& operator=(impulse v);
  ValueImpl& operator=(int v);
  ValueImpl& operator=(float v);
  ValueImpl& operator=(double v);
  ValueImpl& operator=(bool v);
  ValueImpl& operator=(const std::string& v);
  ValueImpl& operator=(std::string&& v);
  ValueImpl& operator=(char v);
  ValueImpl& operator=(vec2f v);
  ValueImpl& operator=(vec3f v);
  ValueImpl& operator=(vec4f v);
  ValueImpl& operator=(const tuple_t& v);
  ValueImpl& operator=(tuple_t&& v);

  const variant_t& impl() const
  {
    return m_variant;
  }

  bool operator==(const ValueImpl& other) const;
  bool operator!=(const ValueImpl& other) const;

  bool isNumeric() const;
  bool isValid() const;
  bool isArray() const;

  ValueType which() const
  {
    return static_cast<ValueType>(m_variant.which());
  }

  template <typename TheType>
  bool is() const
  {
    return m_variant.target<TheType>();
  }

  template <typename TheType>
  auto target() const
  {
    return m_variant.target<TheType>();
  }

  template <typename TheType>
  TheType get() const
  {
    return eggs::variants::get<TheType>(m_variant);
  }

private:
  variant_t m_variant;
};

template <>
inline QString ValueImpl::get<QString>() const
{
  return QString::fromStdString(eggs::variants::get<std::string>(m_variant));
}
template <>
inline QChar ValueImpl::get<QChar>() const
{
  return QChar(eggs::variants::get<char>(m_variant));
}

template <>
inline bool ValueImpl::is<QString>() const
{
  return is<std::string>();
}
template <>
inline bool ValueImpl::is<QChar>() const
{
  return is<char>();
}
/**
 * @brief The Value struct
 *
 * A variant used to represent the data that can be in a message.
 *
 */
struct ISCORE_LIB_STATE_EXPORT Value
{
  using value_type = ValueImpl;
  value_type val{};

  template <typename Val>
  static Value fromValue(Val&& val)
  {
    return State::Value{std::forward<Val>(val)};
  }

  static Value fromValue(const QString& val)
  {
    return State::Value{val.toStdString()};
  }

  static Value fromValue(QString&& val)
  {
    return State::Value{val.toStdString()};
  }

  Value() = default;
  Value(value_type v) : val(std::move(v))
  {
  }
  Value(const Value&) = default;
  Value(Value&&) = default;
  Value& operator=(const Value&) = default;
  Value& operator=(Value&&) = default;

  bool operator==(const Value& m) const;
  bool operator!=(const Value& m) const;
};

using ValueList = QList<Value>;
using OptionalValue = optional<State::Value>;

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m);

// Hopefully we won't need this for much longer.
ISCORE_LIB_STATE_EXPORT ossia::value toOSSIAValue(const State::ValueImpl& val);
ISCORE_LIB_STATE_EXPORT State::Value fromOSSIAValue(const ossia::value& val);
}

Q_DECLARE_METATYPE(State::Value)
Q_DECLARE_METATYPE(State::ValueList)
Q_DECLARE_METATYPE(State::ValueType)
