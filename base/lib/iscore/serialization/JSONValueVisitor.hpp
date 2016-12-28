#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <iscore/serialization/StringConstants.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/EntityBase.hpp>

template <class>
class StringKey;

class JSONValue;
class JSONValueReader;
class JSONValueWriter;


class JSONValue
{
public:
  using Serializer = JSONValueReader;
  using Deserializer = JSONValueWriter;

  // TODO this one isn't part of serialize_dyn, etc.
  static constexpr SerializationIdentifier type()
  {
    return 3;
  }
};

class ISCORE_LIB_BASE_EXPORT JSONValueReader
    : public AbstractVisitor
{
public:
  using is_visitor_tag = std::integral_constant<bool, true>;

  JSONValueReader() = default;
  JSONValueReader(const JSONValueReader&) = delete;
  JSONValueReader& operator=(const JSONValueReader&)
      = delete;

  VisitorVariant toVariant()
  {
    return {*this, JSONValue::type()};
  }

  template <template <class...> class T, typename... Args>
  void readFrom(
      const T<Args...>& obj,
      typename std::enable_if<is_template<T<Args...>>::value, void>::
          type* = nullptr)
  {
    TSerializer<JSONValue, T<Args...>>::readFrom(*this, obj);
  }

  template <
      typename T,
      std::
          enable_if_t<!std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
  void readFrom(const T&);

  template <
      typename T,
      std::
          enable_if_t<std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
  void readFrom(const T& elt)
  {
    val = (int32_t)elt;
  }

  QJsonValue val;
  const iscore::StringConstants& strings{iscore::StringConstant()};
};

class ISCORE_LIB_BASE_EXPORT JSONValueWriter
    : public AbstractVisitor
{
public:
  using is_visitor_tag = std::integral_constant<bool, true>;
  using is_deserializer_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant()
  {
    return {*this, JSONValue::type()};
  }

  JSONValueWriter() = default;
  JSONValueWriter(const JSONValueReader&) = delete;
  JSONValueWriter& operator=(const JSONValueWriter&)
      = delete;

  JSONValueWriter(const QJsonValue& obj) : val{obj}
  {
  }

  JSONValueWriter(QJsonValue&& obj) : val{std::move(obj)}
  {
  }

  template <template <class...> class T, typename... Args>
  void writeTo(
      T<Args...>& obj,
      typename std::enable_if<is_template<T<Args...>>::value, void>::
          type* = nullptr)
  {
    TSerializer<JSONValue, T<Args...>>::writeTo(*this, obj);
  }

  template <
      typename T,
      std::
          enable_if_t<!std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
  void writeTo(T&);
  template <
      typename T,
      std::
          enable_if_t<std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
  void writeTo(T& elt)
  {
    elt = static_cast<T>(val.toInt());
  }

  QJsonValue val;
  const iscore::StringConstants& strings{iscore::StringConstant()};
};

template <>
struct TSerializer<JSONValue, optional<int32_t>>
{
  static void readFrom(JSONValue::Serializer& s, const optional<int32_t>& obj)
  {
    if (obj)
    {
      s.val = *obj;
    }
    else
    {
      s.val = s.strings.none;
    }
  }

  static void writeTo(JSONValue::Deserializer& s, optional<int32_t>& obj)
  {
    if (s.val.toString() == s.strings.none)
    {
      obj = iscore::none;
    }
    else
    {
      obj = s.val.toInt();
    }
  }
};

template <>
struct TSerializer<JSONValue, optional<double>>
{
  static void readFrom(JSONValue::Serializer& s, const optional<double>& obj)
  {
    if (obj)
    {
      s.val = *obj;
    }
    else
    {
      s.val = s.strings.none;
    }
  }

  static void writeTo(JSONValue::Deserializer& s, optional<double>& obj)
  {
    if (s.val.toString() == s.strings.none)
    {
      obj = iscore::none;
    }
    else
    {
      obj = s.val.toDouble();
    }
  }
};

template <typename U>
struct TSerializer<JSONValue, Id<U>>
{
  static void readFrom(JSONValue::Serializer& s, const Id<U>& obj)
  {
    s.val = obj.val();
  }

  static void writeTo(JSONValue::Deserializer& s, Id<U>& obj)
  {
    obj.setVal(s.val.toInt());
  }
};

template <typename U>
struct TSerializer<JSONValue, OptionalId<U>>
{
  static void readFrom(JSONValue::Serializer& s, const OptionalId<U>& obj)
  {
    if (obj)
      s.val = (*obj).val();
    else
      s.val = QJsonValue{};
  }

  static void writeTo(JSONValue::Deserializer& s, OptionalId<U>& obj)
  {
    if (s.val.isNull())
      obj = {};
    else
      obj = Id<U>{s.val.toInt()};
  }
};

template <typename T>
QJsonValue toJsonValue(const T& obj)
{
  JSONValueReader reader;
  reader.readFrom(obj);

  return reader.val;
}

template <typename T>
void fromJsonValue(QJsonValue&& json, T& val)
{
  JSONValueWriter writer{json};
  writer.writeTo(val);
}

template <typename T>
void fromJsonValue(QJsonValue& json, T& val)
{
  JSONValueWriter writer{json};
  writer.writeTo(val);
}

template <typename T>
T fromJsonValue(const QJsonValue& json)
{
  T val;
  JSONValueWriter writer{json};
  writer.writeTo(val);
  return val;
}

template <typename T>
void fromJsonValue(QJsonValueRef&& json, T& val)
{
  JSONValueWriter writer{static_cast<QJsonValue>(json)};
  writer.writeTo(val);
}

template <typename T>
void fromJsonValue(const QJsonValueRef& json, T& val)
{
  JSONValueWriter writer{static_cast<const QJsonValue&>(json)};
  writer.writeTo(val);
}

template <typename T>
T fromJsonValue(QJsonValueRef&& json)
{
  T val;
  JSONValueWriter writer{static_cast<QJsonValue>(json)};
  writer.writeTo(val);
  return val;
}

template <typename T>
T fromJsonValue(const QJsonValueRef& json)
{
  T val;
  JSONValueWriter writer{static_cast<const QJsonValue&>(json)};
  writer.writeTo(val);
  return val;
}

template <template <typename U> class T, typename V>
void fromJsonValueArray(const QJsonArray&& json_arr, T<Id<V>>& arr)
{
  for (const auto& elt : json_arr)
  {
    arr.push_back(fromJsonValue<Id<V>>(elt));
  }
}

template <
    typename Container,
    std::enable_if_t<!std::is_arithmetic<
        typename Container::value_type>::value>* = nullptr>
QJsonArray toJsonValueArray(const Container& c)
{
  QJsonArray arr;

  for (const auto& elt : c)
  {
    arr.push_back(toJsonValue(elt));
  }

  return arr;
}

template <
    typename Container,
    std::enable_if_t<std::is_arithmetic<
        typename Container::value_type>::value>* = nullptr>
QJsonArray toJsonValueArray(const Container& c)
{
  QJsonArray arr;

  for (auto elt : c)
  {
    arr.push_back(elt);
  }

  return arr;
}

template <typename Container>
Container fromJsonValueArray(const QJsonArray& json_arr)
{
  Container c;
  c.reserve(json_arr.size());

  for (const auto& elt : json_arr)
  {
    c.push_back(fromJsonValue<typename Container::value_type>(elt));
  }

  return c;
}



inline QJsonValue toJsonValue(int obj)
{
  return obj;
}
inline QJsonValue toJsonValue(float obj)
{
  return obj;
}
inline QJsonValue toJsonValue(char obj)
{
  return QString(QChar(obj));
}
inline QJsonValue toJsonValue(bool obj)
{
  return obj;
}
template <>
inline QJsonValue toJsonValue<int>(const int& obj)
{
  return obj;
}
template <>
inline QJsonValue toJsonValue<float>(const float& obj)
{
  return obj;
}
template <>
inline QJsonValue toJsonValue<char>(const char& obj)
{
  return QString(QChar(obj));
}
template <>
inline QJsonValue toJsonValue<bool>(const bool& obj)
{
  return obj;
}
template <>
inline QJsonValue toJsonValue<std::array<float, 2>>(const std::array<float, 2>& obj)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < 2; i++)
    arr.push_back(obj[i]);
  return arr;
}
template <>
inline QJsonValue toJsonValue<std::array<float, 3>>(const std::array<float, 3>& obj)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < 3; i++)
    arr.push_back(obj[i]);
  return arr;
}
template <>
inline QJsonValue toJsonValue<std::array<float, 4>>(const std::array<float, 4>& obj)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < 4; i++)
    arr.push_back(obj[i]);
  return arr;
}
inline QJsonValue toJsonValue(const std::string& obj)
{
  return QString::fromStdString(obj);
}

template <>
inline int fromJsonValue<int>(const QJsonValue& obj)
{
  return obj.toInt();
}
template <>
inline float fromJsonValue<float>(const QJsonValue& obj)
{
  return obj.toDouble();
}
template <>
inline char fromJsonValue<char>(const QJsonValue& obj)
{
  auto s = obj.toString();
  return s.isEmpty() ? (char)0 : s[0].toLatin1();
}
template <>
inline bool fromJsonValue<bool>(const QJsonValue& obj)
{
  return obj.toBool();
}
template <>
inline std::string fromJsonValue<std::string>(const QJsonValue& obj)
{
  return obj.toString().toStdString();
}

template <>
inline int fromJsonValue<int>(const QJsonValueRef& obj)
{
  return obj.toInt();
}
template <>
inline float fromJsonValue<float>(const QJsonValueRef& obj)
{
  return obj.toDouble();
}
template <>
inline char fromJsonValue<char>(const QJsonValueRef& obj)
{
  auto s = obj.toString();
  return s.isEmpty() ? (char)0 : s[0].toLatin1();
}
template <>
inline bool fromJsonValue<bool>(const QJsonValueRef& obj)
{
  return obj.toBool();
}
template <>
inline std::string fromJsonValue<std::string>(const QJsonValueRef& obj)
{
  return obj.toString().toStdString();
}


template <typename U>
struct TSerializer<JSONValue, UuidKey<U>>
{
  static void readFrom(JSONValue::Serializer& s, const UuidKey<U>& uid)
  {
    s.readFrom(uid.impl());
  }

  static void writeTo(JSONValue::Deserializer& s, UuidKey<U>& uid)
  {
    s.writeTo(uid.impl());
  }
};

