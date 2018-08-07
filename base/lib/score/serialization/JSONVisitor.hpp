#pragma once
#include <ossia/detail/small_vector.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <score/application/ApplicationComponents.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/StringConstants.hpp>

/**
 * This file contains facilities
 * to serialize an object into a QJsonObject.
 */

template <typename T>
T fromJsonObject(QJsonObject&& json);
template <typename T>
T fromJsonObject(QJsonValue&& json);
template <typename T>
T fromJsonObject(QJsonValueRef json);

class JSONObjectReader;
class JSONObjectWriter;
class JSONObject
{
public:
  using Serializer = JSONObjectReader;
  using Deserializer = JSONObjectWriter;
  static constexpr SerializationIdentifier type()
  {
    return 1;
  }
};

template <class>
class TreeNode;
template <class>
class TreePath;

namespace eggs
{
namespace variants
{
template <class...>
class variant;
}
}

class SCORE_LIB_BASE_EXPORT JSONObjectReader : public AbstractVisitor
{
public:
  using type = JSONObject;
  using is_visitor_tag = std::integral_constant<bool, true>;

  JSONObjectReader();
  JSONObjectReader(const JSONObjectReader&) = delete;
  JSONObjectReader& operator=(const JSONObjectReader&) = delete;

  VisitorVariant toVariant()
  {
    return {*this, JSONObject::type()};
  }

  template <typename T>
  static auto marshall(const T& t)
  {
    JSONObjectReader reader;
    reader.readFrom(t);
    return reader.obj;
  }

  //! Called by code that wants to serialize.
  template <typename T>
  void readFrom(const T& obj)
  {
    readFrom_impl(obj, typename serialization_tag<T>::type{});
  }

  //! Serializable types should reimplement this method
  //! It is not to be called by user code.
  template <typename T>
  void read(const T&);

  QJsonObject obj;

  const score::ApplicationComponents& components;
  const score::StringConstants& strings;

private:
  template <typename T>
  void readFrom_impl(const T& obj, visitor_template_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    TSerializer<JSONObject, T>::readFrom(*this, obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_object_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    else
    {
      TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(*this, obj);
    }

    if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
        TSerializer<JSONObject, T>::readFrom(*this, obj);
    else
        read(obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_entity_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    else
    {
      TSerializer<JSONObject, score::Entity<T>>::readFrom(*this, obj);
    }

    if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
        TSerializer<JSONObject, T>::readFrom(*this, obj);
    else
        read(obj);
  }

  template <typename T, typename Fun>
  void readFromAbstract(const T& in, Fun f)
  {
    obj[strings.uuid] = toJsonValue(in.concreteKey().impl());
    f(*this);
    in.serialize_impl(this->toVariant());
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_tag)
  {
    readFromAbstract(obj, [&](JSONObjectReader& sub) { sub.read(obj); });
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_object_tag)
  {
    readFromAbstract(obj, [&](JSONObjectReader& sub) {
      sub.readFrom_impl(obj, visitor_object_tag{});
    });
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_entity_tag)
  {
    readFromAbstract(obj, [&](JSONObjectReader& sub) {
      sub.readFrom_impl(obj, visitor_entity_tag{});
    });
  }

  //! Used to serialize general objects that won't fit in the other categories
  template <typename T>
  void readFrom_impl(const T& obj, visitor_default_tag)
  {
    read(obj);
  }
};

class SCORE_LIB_BASE_EXPORT JSONObjectWriter : public AbstractVisitor
{
public:
  using type = JSONObject;
  using is_visitor_tag = std::integral_constant<bool, true>;
  using is_deserializer_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant()
  {
    return {*this, JSONObject::type()};
  }

  JSONObjectWriter();
  JSONObjectWriter(const JSONObjectWriter&) = delete;
  JSONObjectWriter& operator=(const JSONObjectWriter&) = delete;

  JSONObjectWriter(const QJsonObject& obj);
  JSONObjectWriter(QJsonObject&& obj);

  template <typename T>
  static auto unmarshall(const QJsonObject& obj)
  {
    T data;
    JSONObjectWriter wrt{obj};
    wrt.writeTo(data);
    return data;
  }

  template <typename T>
  void write(T&);

  template <typename T>
  void writeTo(T& obj)
  {
    writeTo_impl(obj, typename serialization_tag<T>::type{});
  }

  template <typename T>
  T writeTo()
  {
    T val;
    writeTo(val);
    return val;
  }

  const QJsonObject obj;
  const score::ApplicationComponents& components;
  const score::StringConstants& strings;

private:
  template <typename T>
  void writeTo_impl(T& obj, visitor_template_tag)
  {
    TSerializer<JSONObject, T>::writeTo(*this, obj);
  }

  template <typename T, typename OtherTag>
  void writeTo_impl(T& obj, OtherTag)
  {
    write(obj);
  }
};

template <typename T>
struct TSerializer<JSONObject, IdentifiedObject<T>>
{
  template <typename U>
  static void
  readFrom(JSONObject::Serializer& s, const IdentifiedObject<U>& obj)
  {
    s.obj[s.strings.ObjectName] = obj.objectName();
    s.obj[s.strings.id] = obj.id().val();
  }

  template <typename U>
  static void writeTo(JSONObject::Deserializer& s, IdentifiedObject<U>& obj)
  {
    obj.setObjectName(s.obj[s.strings.ObjectName].toString());
    obj.setId(Id<T>{s.obj[s.strings.id].toInt()});
  }
};

template <>
struct TSerializer<JSONObject, optional<int32_t>>
{
  // TODO should not be used. Save as optional json value instead.

  static void readFrom(JSONObject::Serializer& s, const optional<int32_t>& obj)
  {
    if (obj)
    {
      s.obj[s.strings.id] = *obj;
    }
    else
    {
      s.obj[s.strings.id] = s.strings.none;
    }
  }

  static void writeTo(JSONObject::Deserializer& s, optional<int32_t>& obj)
  {
    if (s.obj[s.strings.id].toString() == s.strings.none)
    {
      obj = ossia::none;
    }
    else
    {
      obj = s.obj[s.strings.id].toInt();
    }
  }
};

template <typename T>
QJsonObject toJsonObject(const T& obj)
{
  JSONObjectReader reader;
  reader.readFrom(obj);

  return reader.obj;
}

template <typename T>
void fromJsonObject(QJsonObject&& json, T& obj)
{
  JSONObjectWriter writer{json};
  writer.writeTo(obj);
}

template <typename T>
void fromJsonObject(QJsonValue&& json, T& obj)
{
  JSONObjectWriter writer{json.toObject()};
  writer.writeTo(obj);
}

template <typename T>
void fromJsonObject(QJsonValueRef json, T& obj)
{
  JSONObjectWriter writer{json.toObject()};
  writer.writeTo(obj);
}

template <typename T>
T fromJsonObject(QJsonObject&& json)
{
  T obj;
  JSONObjectWriter writer{json};
  writer.writeTo(obj);

  return obj;
}

template <typename T>
T fromJsonObject(const QJsonObject& json)
{
  T obj;
  JSONObjectWriter writer{json};
  writer.writeTo(obj);

  return obj;
}
template <typename T>
T fromJsonObject(QJsonValue&& json)
{
  return fromJsonObject<T>(json.toObject());
}

template <typename T>
T fromJsonObject(const QJsonValue& json)
{
  return fromJsonObject<T>(json.toObject());
}

template <typename T>
T fromJsonObject(QJsonValueRef json)
{
  return fromJsonObject<T>(json.toObject());
}

template <template <typename...> class Container>
QJsonArray toJsonArray(const Container<int>& array)
{
  QJsonArray arr;

  for (auto elt : array)
  {
    arr.append(elt);
  }

  return arr;
}

template <template <typename...> class Container>
QJsonArray toJsonArray(const Container<unsigned int>& array)
{
  QJsonArray arr;

  for (auto elt : array)
  {
    arr.append(elt);
  }

  return arr;
}

template <class Container>
QJsonArray toJsonArray_sub(const Container& array, std::false_type)
{
  QJsonArray arr;

  for (const auto& elt : array)
  {
    arr.append(toJsonObject(elt));
  }

  return arr;
}

template <class Container>
QJsonArray toJsonArray_sub(const Container& array, std::true_type)
{
  QJsonArray arr;

  for (const auto& elt : array)
  {
    arr.append(toJsonObject(*elt));
  }

  return arr;
}

template <typename Container>
using return_type_of_iterator = typename std::remove_reference<decltype(
    *std::declval<Container>().begin())>::type;

template <class Container>
QJsonArray toJsonArray(const Container& array)
{
  return toJsonArray_sub(
      array, std::is_pointer<return_type_of_iterator<Container>>());
}

template <template <typename U> class T, typename V>
QJsonArray toJsonArray(const T<Id<V>>& array)
{
  QJsonArray arr;

  for (const auto& elt : array)
  {
    arr.append(toJsonValue(elt));
  }

  return arr;
}

template <template <typename U, typename W> class T, typename V, typename Alloc>
QJsonArray toJsonArray(const T<Id<V>, Alloc>& array)
{
  QJsonArray arr;

  for (const auto& elt : array)
  {
    arr.append(toJsonValue(elt));
  }

  return arr;
}

template <
    template <typename U, std::size_t> class T,
    typename V,
    std::size_t N>
QJsonArray toJsonArray(const T<Id<V>, N>& array)
{
  QJsonArray arr;

  for (const auto& elt : array)
  {
    arr.append(toJsonValue(elt));
  }

  return arr;
}

template <typename Value>
QJsonArray toJsonMap(const QMap<double, Value>& map)
{
  QJsonArray arr;

  auto& strings = score::StringConstant();
  for (const auto& key : map.keys())
  {
    QJsonObject obj;
    obj[strings.k] = key;
    obj[strings.v] = map[key];
    arr.append(obj);
  }

  return arr;
}

template <typename Key, typename Value>
QJsonArray toJsonMap(const QMap<Key, Value>& map)
{
  QJsonArray arr;

  auto& strings = score::StringConstant();
  for (const auto& key : map.keys())
  {
    QJsonObject obj;
    obj[strings.k] = *key.val();
    obj[strings.v] = map[key];
    arr.append(obj);
  }

  return arr;
}

template <
    typename Key,
    typename Value,
    std::enable_if_t<std::is_same<bool, Value>::value>* = nullptr>
QMap<Key, Value> fromJsonMap(const QJsonArray& array)
{
  QMap<Key, Value> map;

  auto& strings = score::StringConstant();
  for (const auto& value : array)
  {
    QJsonObject obj = value.toObject();
    map[Key{obj[strings.k].toInt()}] = obj[strings.v].toBool();
  }

  return map;
}

template <typename Key>
QMap<Key, double> fromJsonMap(const QJsonArray& array)
{
  QMap<Key, double> map;

  auto& strings = score::StringConstant();
  for (const auto& value : array)
  {
    QJsonObject obj = value.toObject();
    map[Key{obj[strings.k].toInt()}] = obj[strings.v].toDouble();
  }

  return map;
}

template <>
inline QMap<int32_t, double> fromJsonMap(const QJsonArray& array)
{
  QMap<int32_t, double> map;

  auto& strings = score::StringConstant();
  for (const auto& value : array)
  {
    QJsonObject obj = value.toObject();
    map[obj[strings.k].toInt()] = obj[strings.v].toDouble();
  }

  return map;
}
template <>
inline QMap<double, double> fromJsonMap(const QJsonArray& array)
{
  QMap<double, double> map;

  auto& strings = score::StringConstant();
  for (const auto& value : array)
  {
    QJsonObject obj = value.toObject();
    map[obj[strings.k].toDouble()] = obj[strings.v].toDouble();
  }

  return map;
}

template <template <typename U> class Container>
void fromJsonArray(QJsonArray&& json_arr, Container<int>& arr)
{
  int n = json_arr.size();
  arr.resize(n);
  for (int i = 0; i < n; i++)
  {
    arr[i] = json_arr.at(i).toInt();
  }
}

template <template <typename U> class Container>
void fromJsonArray(QJsonArray&& json_arr, Container<QString>& arr)
{
  arr.clear();
  arr.reserve(json_arr.size());
  Foreach(json_arr, [&](auto elt) { arr.push_back(elt.toString()); });
}

template <template <typename U> class Container, typename T>
void fromJsonArray(QJsonArray&& json_arr, Container<T>& arr)
{
  arr.clear();
  arr.reserve(json_arr.size());
  Foreach(json_arr, [&](auto elt) {
    T obj;
    fromJsonObject(elt.toObject(), obj);
    arr.push_back(obj);
  });
}

template <
    template <typename U, typename V> class Container,
    typename T1,
    typename T2,
    std::enable_if_t<!std::is_arithmetic<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
  arr.clear();
  arr.reserve(json_arr.size());
  Foreach(json_arr, [&](auto elt) {
    T1 obj;
    fromJsonObject(elt.toObject(), obj);
    arr.push_back(obj);
  });
}

template <
    template <typename U, typename V> class Container,
    typename T1,
    typename T2,
    std::enable_if_t<std::is_integral<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
  int n = json_arr.size();
  arr.resize(n);
  for (int i = 0; i < n; i++)
  {
    arr[i] = json_arr.at(i).toInt();
  }
}

template <
    template <typename U, typename V> class Container,
    typename T1,
    typename T2,
    std::enable_if_t<std::is_floating_point<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
  int n = json_arr.size();
  arr.resize(n);
  for (int i = 0; i < n; i++)
  {
    arr[i] = json_arr.at(i).toDouble();
  }
}

inline void fromJsonArray(QJsonArray&& json_arr, QStringList& arr)
{
  int n = json_arr.size();
  arr.clear();
  arr.reserve(n);
  for (int i = 0; i < n; i++)
  {
    arr.push_back(json_arr.at(i).toString());
  }
}

template <typename T>
QJsonArray toJsonArray(const std::vector<T*>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonObject(*v));
  return arr;
}

template <typename T>
void fromJsonArray(
    const QJsonArray& arr, std::vector<T*>& array, QObject* parent)
{
  for (const auto& v : arr)
  {
    auto obj = v.toObject();
    array.push_back(new T{JSONObjectWriter{obj}, parent});
  }
}

template <typename T, std::size_t N>
void fromJsonArray(
    const QJsonArray& arr, ossia::small_vector<T*, N>& array, QObject* parent)
{
  for (const auto& v : arr)
  {
    auto obj = v.toObject();
    array.push_back(new T{JSONObjectWriter{obj}, parent});
  }
}

Q_DECLARE_METATYPE(JSONObjectReader*)
Q_DECLARE_METATYPE(JSONObjectWriter*)
W_REGISTER_ARGTYPE(JSONObjectReader*)
W_REGISTER_ARGTYPE(JSONObjectWriter*)

