#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/for_each.hpp>

#include <QDebug>

#include <eggs/variant.hpp>

/**
 * @file VariantSerialization
 *
 * @brief Used for serialization of eggs::variant classes.
 *
 * This saves the index and the current element, for both JSON and QDataStream,
 * by iterating at compile time up to the "right" point in the variant.
 *
 */

/**
 * @class VariantDataStreamSerializer
 * @see VariantSerialization
 */

/**
 * @class VariantDataStreamDeserializer
 * @see VariantSerialization
 */

/**
 * @class VariantJSONSerializer
 * @see VariantSerialization
 */

/**
 * @class VariantJSONDeserializer
 * @see VariantSerialization
 */

template <typename T>
struct VariantDataStreamSerializer
{
  VariantDataStreamSerializer(DataStream::Serializer& s_p, const T& var_p)
      : s{s_p}, var{var_p}
  {
  }

  DataStream::Serializer& s;
  const T& var;

  bool done = false;

  template <typename TheClass>
  void operator()();
};

template <typename T>
template <typename TheClass>
void VariantDataStreamSerializer<T>::operator()()
{
  // This trickery iterates over all the types in Args...
  // A single type should be serialized, even if we cannot break.
  if (done)
    return;
  if (auto res = var.template target<TheClass>())
  {
    s.stream() << *res;
    done = true;
  }
}

template <typename T>
struct VariantDataStreamDeserializer
{
  VariantDataStreamDeserializer(
      DataStream::Deserializer& s_p, quint64 which_p, T& var_p)
      : s{s_p}, which{which_p}, var{var_p}
  {
  }

  DataStream::Deserializer& s;
  quint64 which;
  T& var;

  quint64 i = 0;
  template <typename TheClass>
  void operator()();
};

template <typename T>
template <typename TheClass>
void VariantDataStreamDeserializer<T>::operator()()
{
  // Here we iterate until we are on the correct type, and we deserialize it.
  if (i++ != which)
    return;

  TheClass data;
  s.stream() >> data;
  var = std::move(data);
}

template <typename... Args>
struct TSerializer<DataStream, eggs::variant<Args...>>
{
  using var_t = eggs::variant<Args...>;
  static void readFrom(DataStream::Serializer& s, const var_t& var)
  {
    s.stream() << (quint64)var.which();

    // TODO this should be an assert.
    if ((quint64)var.which() != (quint64)var.npos)
    {
      ossia::for_each_type<Args...>(
          VariantDataStreamSerializer<var_t>{s, var});
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, var_t& var)
  {
    quint64 which;
    s.stream() >> which;

    if (which != (quint64)var.npos)
    {
      ossia::for_each_type<Args...>(
          VariantDataStreamDeserializer<var_t>{s, which, var});
    }
    s.checkDelimiter();
  }
};

// This part is required because it isn't as straightforward to save variant
// data
// in JSON as it is to save it in a DataStream.
// Basically, each variant member has an associated name that will be the
// key in the JSON parent object. This name is defined by specializing
// template<> class Metadata<Json_k, T>.
// For instance:
// template<> class Metadata<Json_k, score::Address>
// { public: static constexpr const char * get() { return "Address"; } };
// A JSON_METADATA macro is provided for this.

// This allows easy store and retrieval under a familiar name

// TODO add some ASSERT for the variant being set on debug mode. npos case
// should not happen since we have the OptionalVariant.
// TODO qstring and Qt-serializable objects ???
// The _eggs_impl functions are because enum's don't need full-fledged objects
// in json.
template <typename T, std::enable_if_t<!is_value_t<T>::value>* = nullptr>
QJsonValue readFrom_eggs_impl(const T& res)
{
  return toJsonObject(res);
}
template <typename T, std::enable_if_t<is_value_t<T>::value>* = nullptr>
QJsonValue readFrom_eggs_impl(const T& res)
{
  return toJsonValue(res);
}

/**
 * These two methods are because enum's don't need full-fledged objects.
 */
template <typename T, std::enable_if_t<!is_value_t<T>::value>* = nullptr>
auto writeTo_eggs_impl(const QJsonValue& res)
{
  return fromJsonObject<T>(res.toObject());
}
template <typename T, std::enable_if_t<is_value_t<T>::value>* = nullptr>
auto writeTo_eggs_impl(const QJsonValue& res)
{
  return fromJsonValue<T>(res);
}

template <typename T>
struct VariantJSONSerializer
{
  VariantJSONSerializer(JSONObject::Serializer& s_p, const T& var_p)
      : s{s_p}, var{var_p}
  {
  }
  JSONObject::Serializer& s;
  const T& var;

  bool done = false;

  template <typename TheClass>
  void operator()();
};

template <typename T>
template <typename TheClass>
void VariantJSONSerializer<T>::operator()()
{
  if (done)
    return;

  if (auto res = var.template target<TheClass>())
  {
    s.obj[Metadata<Json_k, TheClass>::get()] = readFrom_eggs_impl(*res);
    done = true;
  }
}

template <typename T>
struct VariantJSONDeserializer
{
  VariantJSONDeserializer(JSONObject::Deserializer& s_p, T& var_p)
      : s{s_p}, var{var_p}
  {
  }
  JSONObject::Deserializer& s;
  T& var;

  bool done = false;
  template <typename TheClass>
  void operator()();
};

template <typename T>
template <typename TheClass>
void VariantJSONDeserializer<T>::operator()()
{
  if (done)
    return;

  auto it = s.obj.constFind(Metadata<Json_k, TheClass>::get());
  if (it != s.obj.constEnd())
  {
    var = writeTo_eggs_impl<TheClass>(*it);
    done = true;
  }
}

template <typename... Args>
struct TSerializer<JSONObject, eggs::variant<Args...>>
{
  using var_t = eggs::variant<Args...>;
  static void readFrom(JSONObject::Serializer& s, const var_t& var)
  {
    if ((quint64)var.which() != (quint64)var.npos)
    {
      ossia::for_each_type<Args...>(VariantJSONSerializer<var_t>{s, var});
    }
  }

  static void writeTo(JSONObject::Deserializer& s, var_t& var)
  {
    ossia::for_each_type<Args...>(VariantJSONDeserializer<var_t>{s, var});
  }
};
