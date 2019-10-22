#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
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
    for(const auto& pair : obj)
    {
      s.stream() << pair;
    }
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    std::size_t sz; s.stream() >> sz;
    for(std::size_t i = 0; i < sz; i++) {
      pair_type v;
      s.stream() >> v;
      obj.insert(std::move(v));
    }
  }
};

template <typename T, typename U>
struct TSerializer<JSONValue, ossia::fast_hash_map<T, U>>
{
  using type = ossia::fast_hash_map<T, U>;
  using pair_type = typename type::value_type;
  static void readFrom(JSONValue::Serializer& s, const type& obj)
  {
    QJsonArray arr;
    for(const auto& pair : obj)
    {
      arr.push_back(toJsonValue(pair));
    }
    s.val = std::move(arr);
  }

  static void writeTo(JSONValue::Deserializer& s, type& obj)
  {
    const auto arr = s.val.toArray();
    for(const auto& e : arr) {
      obj.insert(fromJsonValue<pair_type>(e));
    }
  }
};
