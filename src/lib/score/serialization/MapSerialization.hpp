#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

template <typename T, typename U>
struct TSerializer<DataStream, ossia::fast_hash_map<T, U>>
{
  using type = ossia::fast_hash_map<T, U>;
  using pair_type = typename type::value_type;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    std::size_t sz = obj.size();
    s.stream() << sz;
    for (const auto& pair : obj)
    {
      s.stream() << pair;
    }
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    std::size_t sz;
    s.stream() >> sz;
    for (std::size_t i = 0; i < sz; i++)
    {
      pair_type v;
      s.stream() >> v;
      obj.insert(std::move(v));
    }
  }
};

template <typename T, typename U>
struct TSerializer<DataStream, ossia::flat_map<T, U>>
{
  using type = ossia::flat_map<T, U>;
  using pair_type = typename type::value_type;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    std::size_t sz = obj.size();
    s.stream() << sz;
    for (const auto& pair : obj)
    {
      s.stream() << pair;
    }
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    std::size_t sz;
    s.stream() >> sz;
    for (std::size_t i = 0; i < sz; i++)
    {
      pair_type v;
      s.stream() >> v;
      obj.insert(std::move(v));
    }
  }
};

template <typename T, typename U>
struct TSerializer<JSONObject, ossia::flat_map<T, U>>
{
  using type = ossia::flat_map<T, U>;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    ArraySerializer::readFrom(s, obj.container);
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    ArraySerializer::writeTo(s, obj.container);
  }
};
