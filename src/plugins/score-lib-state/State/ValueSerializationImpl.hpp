#pragma once
#include <State/OSSIASerializationImpl.hpp>

#include <ossia/detail/size.hpp>

#include <brigand/sequences/list.hpp>

//////////// Value Variant serialization /////////////
template <typename Functor>
void apply_typeonly(
    Functor&& functor,
    ossia::value_variant_type::Type type,
    ossia::value_variant_type& var)
{
  using namespace ossia;
  switch (type)
  {
    case value_variant_type::Type::Type0:
      return functor(typeholder<float>{}, var);
    case value_variant_type::Type::Type1:
      return functor(typeholder<int>{}, var);
    case value_variant_type::Type::Type2:
      return functor(typeholder<ossia::vec2f>{}, var);
    case value_variant_type::Type::Type3:
      return functor(typeholder<ossia::vec3f>{}, var);
    case value_variant_type::Type::Type4:
      return functor(typeholder<ossia::vec4f>{}, var);
    case value_variant_type::Type::Type5:
      return functor(typeholder<ossia::impulse>{}, var);
    case value_variant_type::Type::Type6:
      return functor(typeholder<bool>{}, var);
    case value_variant_type::Type::Type7:
      return functor(typeholder<std::string>{}, var);
    case value_variant_type::Type::Type8:
      return functor(typeholder<std::vector<ossia::value>>{}, var);
    case value_variant_type::Type::Type9:
      return functor(typeholder<char>{}, var);
    default:
      throw;
  }
}

struct ValueVariantJsonSerializer
{
  QJsonObject& m_obj;
  template <typename T>
  void operator()(const T& value)
  {
    m_obj[Metadata<Json_k, T>::get()] = toJsonValue(value);
  }
};

struct ValueVariantDatastreamSerializer
{
  DataStream::Serializer& s;
  template <typename T>
  void operator()(const T& value)
  {
    s.stream() << value;
  }
};
template <>
struct TSerializer<DataStream, ossia::value_variant_type>
{
  using var_t = ossia::value_variant_type;
  static void readFrom(DataStream::Serializer& s, const var_t& var)
  {
    s.stream() << (quint64)var.which();

    if (var)
    {
      ossia::apply_nonnull(ValueVariantDatastreamSerializer{s}, var);
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, var_t& var)
  {
    quint64 which;
    s.stream() >> which;

    if (which != (quint64)var.npos)
    {
      apply_typeonly(
          [&](auto type, var_t& var) {
            typename decltype(type)::type value;
            s.stream() >> value;
            var = std::move(value);
          },
          (var_t::Type)which,
          var);
    }
    s.checkDelimiter();
  }
};

template <>
struct TSerializer<JSONObject, ossia::value_variant_type>
{
  using var_t = ossia::value_variant_type;
  static void readFrom(JSONObject::Serializer& s, const var_t& var)
  {
    if (var)
    {
      ossia::apply_nonnull(ValueVariantJsonSerializer{s.obj}, var);
    }
  }

  using value_type_list = brigand::list<
      float,
      int,
      ossia::vec2f,
      ossia::vec3f,
      ossia::vec4f,
      ossia::impulse,
      bool,
      std::string,
      std::vector<ossia::value>,
      char>;

  static auto init_keys()
  {
    std::array<QString, ossia::size<value_type_list>::value> arr;
    int i = 0;
    ossia::for_each_tagged(value_type_list{}, [&](auto t) {
      using type = typename decltype(t)::type;
      arr[i] = Metadata<Json_k, type>::get();
      i++;
    });
    return arr;
  }
  static const auto& keys_list()
  {
    static const auto arr = init_keys();
    return arr;
  }

  static void writeTo(JSONObject::Deserializer& s, var_t& var)
  {
    const auto& keys = keys_list();
    for (std::size_t i = 0; i < keys.size(); i++)
    {
      auto it = s.obj.constFind(keys[i]);
      if (it != s.obj.constEnd())
      {
        apply_typeonly(
            [&](auto type, var_t& var) {
              var = fromJsonValue<typename decltype(type)::type>(*it);
            },
            (var_t::Type)i,
            var);
        return;
      }
    }
  }
};

/////////:

template <>
void DataStreamReader::read(const ossia::impulse& value)
{
}

template <>
void DataStreamWriter::write(ossia::impulse& value)
{
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::value& n)
{
  writeTo((ossia::value_variant_type&)n.v);
}

/// JSON ///
template <>
ossia::impulse fromJsonValue<ossia::impulse>(const QJsonValueRef& obj)
{
  return {};
}

QJsonArray toJsonArray(const std::vector<ossia::value>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

QJsonArray toJsonArray(const std::vector<ossia::flat_set<ossia::value>>& array)
{
  QJsonArray arr;
  for (auto& v : array)
  {
    QJsonArray sub;
    for (auto& val : v)
      sub.push_back(toJsonValue(val));
    arr.push_back(std::move(sub));
  }
  return arr;
}
void fromJsonArray(const QJsonArray& arr, std::vector<ossia::value>& array)
{
  array.reserve(arr.size());
  for (const auto& val : arr)
  {
    array.push_back(fromJsonValue<ossia::value>(val));
  }
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectReader::read(const ossia::value& n);

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectWriter::write(ossia::value& n);

template <>
SCORE_LIB_STATE_EXPORT void JSONValueReader::read(const ossia::value& n);
template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(ossia::value& n);

template <>
struct TSerializer<JSONObject, std::vector<ossia::value>>
{
  static void
  readFrom(JSONObject::Serializer& s, const std::vector<ossia::value>& vec)
  {
    s.obj[s.strings.Values] = toJsonArray(vec);
  }

  static void
  writeTo(JSONObject::Deserializer& s, std::vector<ossia::value>& vec)
  {
    fromJsonArray(s.obj[s.strings.Values].toArray(), vec);
  }
};

template <>
struct TSerializer<JSONValue, std::vector<ossia::value>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::vector<ossia::value>& vec)
  {
    s.val = toJsonArray(vec);
  }

  static void
  writeTo(JSONValue::Deserializer& s, std::vector<ossia::value>& vec)
  {
    fromJsonArray(s.val.toArray(), vec);
  }
};
