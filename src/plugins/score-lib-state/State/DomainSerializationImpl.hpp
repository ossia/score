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
    s.stream.StartObject();
    s.obj[s.strings.Min] = domain.min;
    s.obj[s.strings.Max] = domain.max;
    s.obj[s.strings.Values] = domain.values;
    s.stream.EndObject();
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    if (auto it = s.obj.tryGet(s.strings.Min))
    {
      domain.min = *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Max))
    {
      domain.max = *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Values))
    {
      domain.values = *it;
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
    s.stream.StartObject();

    if (domain.min)
      s.obj[s.strings.Min] = *domain.min;
    if (domain.max)
      s.obj[s.strings.Max] = *domain.max;
    if (!domain.values.empty())
      s.obj[s.strings.Values] = domain.values;

    s.stream.EndObject();
  }

  static void writeTo(const JSONObject::Deserializer& s, domain_t& domain)
  {
    using val_t = typename domain_t::value_type;
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    if (auto it = s.obj.tryGet(s.strings.Min))
    {
      domain.min = *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Max))
    {
      domain.max = *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Values))
    {
      domain.values = *it;
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<std::string>>
{
  using domain_t = ossia::domain_base<std::string>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.stream.StartObject();
    if (!domain.values.empty())
      s.obj[s.strings.Values] = domain.values;
    s.stream.EndObject();
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    if (auto it_values = s.obj.tryGet(s.strings.Values))
    {
      domain.values <<= *it_values;
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<ossia::impulse>>
{
  using domain_t = ossia::domain_base<ossia::impulse>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.stream.Null();
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain) {}
};

template <>
struct TSerializer<JSONObject, ossia::domain_base<bool>>
{
  using domain_t = ossia::domain_base<bool>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.stream.Null();
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain) {}
};

template <>
struct TSerializer<JSONObject, ossia::vector_domain>
{
  using domain_t = ossia::vector_domain;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.stream.StartObject();
    s.obj[s.strings.Min] = domain.min;
    s.obj[s.strings.Max] = domain.max;
    s.obj[s.strings.Values] = domain.values;
    s.stream.EndObject();
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...

    if (auto it = s.obj.tryGet(s.strings.Min))
    {
      domain.min <<= *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Max))
    {
      domain.max <<= *it;
    }
    if (auto it = s.obj.tryGet(s.strings.Values))
    {
      domain.values <<= *it;
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

template <>
struct TSerializer<DataStream, ossia::domain_base_variant>
{
  using var_t = ossia::domain_base_variant;
  static void readFrom(DataStream::Serializer& s, const var_t& var)
  {
    s.stream() << (quint64)var.which();

    if (var)
    {
      ossia::apply_nonnull([&] (const auto& v) { s.stream() << v; }, var);
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
    s.stream.StartObject();
    if (var)
    {
      ossia::apply_nonnull([&] (const auto& domain) {
        using type = std::remove_const_t<std::remove_reference_t<decltype(domain)>>;
        s.obj[Metadata<Json_k, type>::get()] = domain;

      }, var);
    }
    s.stream.EndObject();
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
    if(!s.base.IsObject() || s.base.MemberCount() == 0)
      return;
    const auto& keys = keys_list();
    for (std::size_t i = 0; i < keys.size(); i++)
    {
      auto it = s.obj.constFind(keys[i]);
      if (it != s.obj.constEnd())
      {
        apply_typeonly(
            [&](auto type, var_t& var) {
              var <<= *it;
            },
            (var_t::Type)i,
            var);
        return;
      }
    }
  }
};
