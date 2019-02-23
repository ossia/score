#pragma once
#include <State/OSSIASerializationImpl.hpp>
#include <State/ValueSerializationImpl.hpp>

//////////// Domain Variant serialization /////////////

template <>
struct TSerializer<DataStream, ossia::vector_domain>
{
  using domain_t = ossia::vector_domain;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    s.stream() << domain.min << domain.max << domain.values;
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    s.stream() >> domain.min >> domain.max >> domain.values;
  }
};

template <std::size_t N>
struct TSerializer<JSONObject, ossia::vecf_domain<N>>
{
  using domain_t = ossia::vecf_domain<N>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.obj[s.strings.Min] = toJsonArray(domain.min);
    s.obj[s.strings.Max] = toJsonArray(domain.max);
    s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<std::array<optional<float>, N>>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<std::array<optional<float>, N>>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      std::size_t i = 0;
      for (const auto& v : arr)
      {
        if (i < N)
        {
          if (v.isArray())
          {
            for (const auto& u : v.toArray())
              domain.values[i].insert(fromJsonValue<float>(u));
          }
          i++;
        }
      }
    }
  }
};

template <std::size_t N>
struct TSerializer<DataStream, ossia::vecf_domain<N>>
{
  using domain_t = ossia::vecf_domain<N>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    s.stream() << domain.min << domain.max << domain.values;
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    s.stream() >> domain.min >> domain.max >> domain.values;
  }
};

template <typename T>
struct TSerializer<JSONObject, ossia::domain_base<T>>
{
  using domain_t = ossia::domain_base<T>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    if (domain.min)
      s.obj[s.strings.Min] = toJsonValue(*domain.min);
    if (domain.max)
      s.obj[s.strings.Max] = toJsonValue(*domain.max);
    if (!domain.values.empty())
      s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(const JSONObject::Deserializer& s, domain_t& domain)
  {
    using val_t = typename domain_t::value_type;
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<val_t>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<val_t>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      for (const auto& v : arr)
      {
        domain.values.insert(fromJsonValue<val_t>(v));
      }
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<std::string>>
{
  using domain_t = ossia::domain_base<std::string>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    if (!domain.values.empty())
      s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      for (const auto& v : arr)
      {
        domain.values.insert(fromJsonValue<std::string>(v));
      }
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<ossia::impulse>>
{
  using domain_t = ossia::domain_base<ossia::impulse>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain) {}

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain) {}
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<bool>>
{
  using domain_t = ossia::domain_base<bool>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain) {}

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain) {}
};

template <>
struct TSerializer<JSONObject, ossia::vector_domain>
{
  using domain_t = ossia::vector_domain;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.obj[s.strings.Min] = toJsonArray(domain.min);
    s.obj[s.strings.Max] = toJsonArray(domain.max);
    s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<std::vector<ossia::value>>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<std::vector<ossia::value>>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      domain.values.resize(arr.size());
      int i = 0;
      for (const auto& v : arr)
      {
        if (v.isArray())
        {
          for (const auto& u : v.toArray())
            domain.values[i].insert(fromJsonValue<ossia::value>(u));
        }
        i++;
      }
    }
  }
};

template <typename T>
struct TSerializer<DataStream, ossia::domain_base<T>>
{
  using domain_t = ossia::domain_base<T>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    // Min
    {
      bool min_b{domain.min};
      s.stream() << min_b;
      if (min_b)
        s.stream() << *domain.min;
    }

    // Max
    {
      bool max_b{domain.max};
      s.stream() << max_b;
      if (max_b)
        s.stream() << *domain.max;
    }

    // Values
    {
      s.stream() << (int32_t)domain.values.size();
      for (auto& val : domain.values)
        s.stream() << val;
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    {
      bool min_b;
      s.stream() >> min_b;
      if (min_b)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.min = std::move(v);
      }
    }

    {
      bool max_b;
      s.stream() >> max_b;
      if (max_b)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.max = std::move(v);
      }
    }

    {
      int32_t count;
      s.stream() >> count;
      for (int i = 0; i < count; i++)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.values.insert(v);
      }
    }

    s.checkDelimiter();
  }
};

template <>
struct TSerializer<DataStream, ossia::domain_base<std::string>>
{
  using domain_t = ossia::domain_base<std::string>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    // Values
    {
      s.stream() << (int32_t)domain.values.size();
      for (auto& val : domain.values)
        s.stream() << val;
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    {
      int32_t count;
      s.stream() >> count;
      for (int i = 0; i < count; i++)
      {
        std::string v;
        s.stream() >> v;
        domain.values.insert(v);
      }
    }

    s.checkDelimiter();
  }
};

template <>
struct TSerializer<DataStream, ossia::domain_base<ossia::impulse>>
{
  using domain_t = ossia::domain_base<ossia::impulse>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain) {}

  static void writeTo(DataStream::Deserializer& s, domain_t& domain) {}
};

template <>
struct TSerializer<DataStream, ossia::domain_base<bool>>
{
  using domain_t = ossia::domain_base<bool>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain) {}

  static void writeTo(DataStream::Deserializer& s, domain_t& domain) {}
};

template <typename Functor>
void apply_typeonly(
    Functor&& functor,
    ossia::domain_base_variant::Type type,
    ossia::domain_base_variant& var)
{
  using namespace ossia;
  switch (type)
  {
    case domain_base_variant::Type::Type0:
      return functor(typeholder<ossia::domain_base<ossia::impulse>>{}, var);
    case domain_base_variant::Type::Type1:
      return functor(typeholder<ossia::domain_base<bool>>{}, var);
    case domain_base_variant::Type::Type2:
      return functor(typeholder<ossia::domain_base<int>>{}, var);
    case domain_base_variant::Type::Type3:
      return functor(typeholder<ossia::domain_base<float>>{}, var);
    case domain_base_variant::Type::Type4:
      return functor(typeholder<ossia::domain_base<char>>{}, var);
    case domain_base_variant::Type::Type5:
      return functor(typeholder<ossia::domain_base<std::string>>{}, var);
    case domain_base_variant::Type::Type6:
      return functor(typeholder<ossia::vector_domain>{}, var);
    case domain_base_variant::Type::Type7:
      return functor(typeholder<ossia::vecf_domain<2>>{}, var);
    case domain_base_variant::Type::Type8:
      return functor(typeholder<ossia::vecf_domain<3>>{}, var);
    case domain_base_variant::Type::Type9:
      return functor(typeholder<ossia::vecf_domain<4>>{}, var);
    case domain_base_variant::Type::Type10:
      return functor(typeholder<ossia::domain_base<ossia::value>>{}, var);
    default:
      throw;
  }
}
struct DomainVariantJsonSerializer
{
  QJsonObject& m_obj;
  template <typename T>
  void operator()(const T& value)
  {
    m_obj[Metadata<Json_k, T>::get()] = toJsonObject(value);
  }
};

struct DomainVariantDatastreamSerializer
{
  DataStream::Serializer& s;
  template <typename T>
  void operator()(const T& value)
  {
    s.stream() << value;
  }
};
template <>
struct TSerializer<DataStream, ossia::domain_base_variant>
{
  using var_t = ossia::domain_base_variant;
  static void readFrom(DataStream::Serializer& s, const var_t& var)
  {
    s.stream() << (quint64)var.which();

    if (var)
    {
      ossia::apply_nonnull(DomainVariantDatastreamSerializer{s}, var);
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
struct TSerializer<JSONObject, ossia::domain_base_variant>
{
  using var_t = ossia::domain_base_variant;
  static void readFrom(JSONObject::Serializer& s, const var_t& var)
  {
    if (var)
    {
      ossia::apply_nonnull(DomainVariantJsonSerializer{s.obj}, var);
    }
  }

  using value_type_list = brigand::list<
      ossia::domain_base<ossia::impulse>,
      ossia::domain_base<bool>,
      ossia::domain_base<int>,
      ossia::domain_base<float>,
      ossia::domain_base<char>,
      ossia::domain_base<std::string>,
      ossia::vector_domain,
      ossia::vecf_domain<2>,
      ossia::vecf_domain<3>,
      ossia::vecf_domain<4>,
      ossia::domain_base<ossia::value>>;

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
              var = fromJsonObject<typename decltype(type)::type>(*it);
            },
            (var_t::Type)i,
            var);
        return;
      }
    }
  }
};
