#pragma once
#include <score/model/EntityBase.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/serialization/VisitorTags.hpp>

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
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
  static constexpr SerializationIdentifier type() { return 3; }
};

class SCORE_LIB_BASE_EXPORT JSONValueReader : public AbstractVisitor
{
public:
  using is_visitor_tag = std::integral_constant<bool, true>;

  JSONValueReader() = default;
  JSONValueReader(const JSONValueReader&) = delete;
  JSONValueReader& operator=(const JSONValueReader&) = delete;

  VisitorVariant toVariant() { return {*this, JSONValue::type()}; }

  template <typename T>
  void readFrom(const T& obj)
  {
    readFrom_impl(obj, typename serialization_tag<T>::type{});
  }

  template <typename T>
  void read(const T&);

  template <typename T>
  void readFrom_impl(const T& obj, visitor_template_tag)
  {
    TSerializer<JSONValue, T>::readFrom(*this, obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_default_tag)
  {
    read(obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_enum_tag)
  {
    val = (int32_t)obj;
  }

  QJsonValue val;
  const score::StringConstants& strings{score::StringConstant()};
};

class SCORE_LIB_BASE_EXPORT JSONValueWriter : public AbstractVisitor
{
public:
  using is_visitor_tag = std::integral_constant<bool, true>;
  using is_deserializer_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant() { return {*this, JSONValue::type()}; }

  JSONValueWriter() = default;
  JSONValueWriter(const JSONValueReader&) = delete;
  JSONValueWriter& operator=(const JSONValueWriter&) = delete;

  JSONValueWriter(const QJsonValue& obj) : val{obj} {}

  JSONValueWriter(QJsonValue&& obj) : val{std::move(obj)} {}

  template <typename T>
  void writeTo(T& obj)
  {
    writeTo_impl(obj, typename serialization_tag<T>::type{});
  }
  QJsonValue val;
  const score::StringConstants& strings{score::StringConstant()};

private:
  template <template <class...> class T, typename... Args>
  void write(
      T<Args...>& obj,
      typename std::enable_if<is_template<T<Args...>>::value, void>::
          type* = nullptr)
  {
    TSerializer<JSONValue, T<Args...>>::writeTo(*this, obj);
  }

  template <typename T>
  void write(T&);

  template <typename T>
  void writeTo_impl(T& obj, visitor_template_tag)
  {
    TSerializer<JSONValue, T>::writeTo(*this, obj);
  }

  template <typename T, typename OtherTag>
  void writeTo_impl(T& obj, OtherTag)
  {
    write(obj);
  }

  template <typename T>
  void writeTo_impl(T& elt, visitor_enum_tag)
  {
    elt = static_cast<T>(val.toInt());
  }
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
      obj = ossia::none;
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
      obj = ossia::none;
    }
    else
    {
      obj = s.val.toDouble();
    }
  }
};

template <>
struct TSerializer<JSONValue, QRectF>
{
  static void readFrom(JSONValue::Serializer& s, const QRectF& obj)
  {
    s.val = QJsonArray{obj.x(), obj.y(), obj.width(), obj.height()};
  }

  static void writeTo(JSONValue::Deserializer& s, QRectF& obj)
  {
    auto arr = s.val.toArray();
    SCORE_ASSERT(arr.size() == 4);
    obj = {arr[0].toDouble(),
           arr[1].toDouble(),
           arr[2].toDouble(),
           arr[3].toDouble()};
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
/*
template <typename U>
struct TSerializer<JSONValue, Path<U>>
{
  static void readFrom(JSONValue::Serializer& s, const Path<U>& obj)
  {
    s.val = toJsonArray(obj.unsafePath().vec());
  }

  static void writeTo(JSONValue::Deserializer& s, Path<U>& obj)
  {
    fromJsonArray(s.val.toArray(), obj.unsafePath().vec());
  }
};
*/

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
    if (s.val.isNull() || s.val.toString() == "none")
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

template <template <typename, typename> class T, typename V, typename Alloc>
void fromJsonValueArray(const QJsonArray& json_arr, T<Id<V>, Alloc>& arr)
{
  arr.reserve(json_arr.size());
  for (const auto& elt : json_arr)
  {
    arr.push_back(fromJsonValue<Id<V>>(elt));
  }
}

#if defined(OSSIA_SMALL_VECTOR)
template <typename V, std::size_t N>
void fromJsonValueArray(
    const QJsonArray& json_arr,
    ossia::small_vector<Id<V>, N>& arr)
{
  arr.reserve(json_arr.size());
  for (const auto& elt : json_arr)
  {
    arr.push_back(fromJsonValue<Id<V>>(elt));
  }
}

template <typename V, std::size_t N>
void fromJsonValueArray(
    const QJsonArray& json_arr,
    ossia::static_vector<Id<V>, N>& arr)
{
  arr.reserve(json_arr.size());
  for (const auto& elt : json_arr)
  {
    arr.push_back(fromJsonValue<Id<V>>(elt));
  }
}
#endif

template <
    typename Container,
    std::enable_if_t<
        !std::is_arithmetic<typename Container::value_type>::value>* = nullptr>
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
    std::enable_if_t<
        std::is_arithmetic<typename Container::value_type>::value>* = nullptr>
QJsonArray toJsonValueArray(const Container& c)
{
  QJsonArray arr;

  for (auto elt : c)
  {
    arr.push_back(elt);
  }

  return arr;
}

template <typename T, std::size_t N>
QJsonArray toJsonValueArray(const ossia::small_vector<T, N>& c)
{
  QJsonArray arr;

  for (const auto& elt : c)
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
  return (double)obj;
}
inline QJsonValue toJsonValue(double obj)
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
  return (double)obj;
}
template <>
inline QJsonValue toJsonValue<double>(const double& obj)
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
inline QJsonValue
toJsonValue<std::array<float, 2>>(const std::array<float, 2>& obj)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < 2; i++)
    arr.push_back(obj[i]);
  return arr;
}
template <>
inline QJsonValue
toJsonValue<std::array<float, 3>>(const std::array<float, 3>& obj)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < 3; i++)
    arr.push_back(obj[i]);
  return arr;
}
template <>
inline QJsonValue
toJsonValue<std::array<float, 4>>(const std::array<float, 4>& obj)
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
  return (float)obj.toDouble();
}
template <>
inline double fromJsonValue<double>(const QJsonValue& obj)
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

template <typename T, typename Alloc>
struct TSerializer<JSONValue, std::vector<T, Alloc>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::vector<T, Alloc>& vec)
  {
    QJsonArray arr;
    for (const auto& e : vec)
      arr.append(toJsonValue(e));
    s.val = std::move(arr);
  }

  static void writeTo(JSONValue::Deserializer& s, std::vector<T, Alloc>& vec)
  {
    const QJsonArray arr = s.val.toArray();
    vec.reserve(arr.size());
    for (const auto& e : arr)
      vec.push_back(fromJsonValue<T>(e));
  }
};

template <typename T, std::size_t N>
struct TSerializer<JSONValue, std::array<T, N>>
{
  static void readFrom(JSONValue::Serializer& s, const std::array<T, N>& vec)
  {
    QJsonArray arr;
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(toJsonValue(vec[i]));
    s.val = std::move(arr);
  }

  static void writeTo(JSONValue::Deserializer& s, std::array<T, N>& vec)
  {
    auto arr = s.val.toArray();
    const std::size_t M = std::min((int)N, arr.size());
    for (std::size_t i = 0; i < M; i++)
      vec[i] = fromJsonValue<T>(arr[i]);
  }
};

template <std::size_t N>
struct TSerializer<JSONValue, std::array<float, N>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::array<float, N>& vec)
  {
    QJsonArray arr;
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(vec[i]);
    s.val = std::move(arr);
  }

  static void writeTo(JSONValue::Deserializer& s, std::array<float, N>& vec)
  {
    auto arr = s.val.toArray();
    const std::size_t M = std::min((int)N, arr.size());
    for (std::size_t i = 0; i < M; i++)
      vec[i] = arr[i].toDouble();
  }
};

template <typename T>
struct TSerializer<JSONValue, optional<T>>
{
  static void readFrom(JSONValue::Serializer& s, const optional<T>& obj)
  {
    if (obj)
    {
      s.val = toJsonValue(*obj);
    }
    else
    {
      s.val = QJsonValue{};
    }
  }

  static void writeTo(JSONValue::Deserializer& s, optional<T>& obj)
  {
    if (s.val.isNull() || s.val.toString() == s.strings.none)
    {
      obj = ossia::none;
    }
    else
    {
      obj = fromJsonValue<T>(s.val);
    }
  }
};

inline QJsonValue toJsonValue(const optional<float>& f)
{
  if (f)
    return *f;
  else
    return QJsonValue{};
}

template <typename T>
QJsonArray toJsonArray(const ossia::flat_set<T>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

inline QJsonArray toJsonArray(const ossia::flat_set<int>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(v);
  return arr;
}
inline QJsonArray toJsonArray(const ossia::flat_set<float>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(v);
  return arr;
}

template <std::size_t N>
QJsonArray toJsonArray(const std::array<optional<float>, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    if (v)
      arr.push_back(*v);
    else
      arr.push_back(QJsonValue{});
  return arr;
}

template <std::size_t N>
QJsonArray toJsonArray(const std::array<ossia::flat_set<float>, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
  {
    QJsonArray sub;
    for (float val : v)
      sub.push_back(val);
    arr.push_back(std::move(sub));
  }
  return arr;
}

template <typename T, std::size_t N>
QJsonArray toJsonArray(const ossia::small_vector<Id<T>, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

template <typename T, std::size_t N>
QJsonArray toJsonArray(const ossia::small_vector<T*, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonObject(*v));
  return arr;
}

template <>
struct TSerializer<JSONValue, std::string>
{
  static void readFrom(JSONValue::Serializer& s, const std::string& v)
  {
    s.val = QString::fromStdString(v);
  }

  static void writeTo(JSONValue::Deserializer& s, std::string& val)
  {
    val = s.val.toString().toStdString();
  }
};

template <typename T, typename U>
struct TSerializer<JSONValue, std::pair<T, U>>
{
  using type = std::pair<T, U>;
  static void readFrom(JSONValue::Serializer& s, const type& obj)
  {
    QJsonArray arr;
    arr.append(toJsonValue(obj.first));
    arr.append(toJsonValue(obj.second));
    s.val = std::move(arr);
  }

  static void writeTo(JSONValue::Deserializer& s, type& obj)
  {
    const auto arr = s.val.toArray();
    obj.first = fromJsonValue<T>(arr[0]);
    obj.second = fromJsonValue<U>(arr[1]);
  }
};
