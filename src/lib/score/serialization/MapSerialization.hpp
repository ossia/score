#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <boost/container/flat_map.hpp>

#include <unordered_map>

struct MapSerializer
{
  template <typename Map_T>
  static void readFrom(DataStream::Serializer& s, const Map_T& obj)
  {
    auto& st = s.stream();
    st << (int32_t)obj.size();
    for(const auto& e : obj)
    {
      st << e.first << e.second;
    }
  }

  template <typename Map_T>
  static void writeTo(DataStream::Deserializer& s, Map_T& obj)
  {
    obj.clear();

    auto& st = s.stream();
    int32_t n;
    st >> n;
    for(int32_t i = 0; i < n; i++)
    {
      typename Map_T::key_type key;
      typename Map_T::mapped_type value;
      st >> key >> value;
      obj.emplace(std::move(key), std::move(value));
    }
  }

  template <typename Map_T>
  static void readFrom(JSONObject::Serializer& s, const Map_T& obj)
  {
    s.stream.StartArray();
    for(const auto& pair : obj)
    {
      s.stream.StartArray();
      s.readFrom(pair.first);
      s.readFrom(pair.second);
      s.stream.EndArray();
    }
    s.stream.EndArray();
  }

  template <typename Map_T>
  static void writeTo(JSONObject::Deserializer& s, Map_T& obj)
  {
    obj.clear();

    const auto& arr = s.base.GetArray();
    for(const auto& elt : arr)
    {
      const auto& pair = elt.GetArray();
      typename Map_T::key_type key;
      typename Map_T::mapped_type value;
      key <<= JsonValue{pair[0]};
      value <<= JsonValue{pair[1]};
      obj.emplace(std::move(key), std::move(value));
    }
  }
};

template <typename T, typename U, typename H, typename E, typename A>
struct TSerializer<DataStream, std::unordered_map<T, U, H, E, A>> : MapSerializer
{
};
template <typename T, typename U, typename H, typename E, typename A>
struct TSerializer<JSONObject, std::unordered_map<T, U, H, E, A>> : MapSerializer
{
};

#if !defined(OSSIA_NO_FAST_CONTAINERS)
template <class Key,
          class T,
          class Hash,
          class KeyEqual,
          class AllocatorOrContainer,
          class Bucket,
          bool IsSegmented>
struct TSerializer<DataStream, ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, IsSegmented>>
    : MapSerializer
{
};

template <class Key,
          class T,
          class Hash,
          class KeyEqual,
          class AllocatorOrContainer,
          class Bucket,
          bool IsSegmented>
struct TSerializer<JSONObject, ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, IsSegmented>>
    : MapSerializer
{
};
#endif

template <typename T, typename U>
struct TSerializer<DataStream, boost::container::dtl::pair<T, U>>
{
  using type = boost::container::dtl::pair<T, U>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.m_stream << obj.first << obj.second;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    s.m_stream >> obj.first >> obj.second;
  }
};

template <typename T, typename U>
struct TSerializer<JSONObject, boost::container::dtl::pair<T, U>>
{
  using type = boost::container::dtl::pair<T, U>;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    s.stream.StartArray();
    s.readFrom(obj.first);
    s.readFrom(obj.second);
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    const auto& arr = s.base.GetArray();
    obj.first <<= JsonValue{arr[0]};
    obj.second <<= JsonValue{arr[1]};
  }
};
template <typename T, typename U>
struct TSerializer<DataStream, ossia::flat_map<T, U>>
{
  using type = ossia::flat_map<T, U>;
  using pair_type = typename type::value_type;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.m_stream << obj.tree().get_sequence_cref();
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    s.m_stream >> obj.tree().get_sequence_ref();
  }
};

template <typename T, typename U>
struct TSerializer<JSONObject, ossia::flat_map<T, U>>
{
  using type = ossia::flat_map<T, U>;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    ArraySerializer::readFrom(s, obj.tree().get_sequence_cref());
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    ArraySerializer::writeTo(s, obj.tree().get_sequence_ref());
  }
};

template <typename T, typename U>
struct TSerializer<DataStream, boost::container::flat_map<T, U>> : MapSerializer
{
};

template <typename T, typename U>
struct TSerializer<JSONObject, boost::container::flat_map<T, U>> : MapSerializer
{
};
