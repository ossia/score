#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/String.hpp>

#include <ossia/detail/any_map.hpp>

/**
 * \file AnySerialization.hpp
 * \brief Serialization mechanism for ossia::any
 *
 * This file provides a method to save data registered in a ossia::any.
 * By default some types will be serialized / deserialized without problems.
 *
 * For the others, one should register them in anySerializers()
 *
 *
 * \todo See the one used in boost.spirit which is said to be much faster.
 */
namespace score
{
using any_map = ossia::any_map;

struct SCORE_LIB_BASE_EXPORT any_serializer
{
  virtual ~any_serializer();
  virtual void apply(DataStream::Serializer&, const ossia::any&) = 0;
  virtual void apply(DataStream::Deserializer&, ossia::any&) = 0;
  virtual void apply(JSONObject::Serializer&, const std::string& key, const ossia::any&) = 0;
  virtual void apply(JSONObject::Deserializer&, const std::string& key, ossia::any&) = 0;
  void cast_error(const char*);
};

template <typename T>
struct any_serializer_t final : public any_serializer
{
  void apply(DataStream::Serializer& s, const ossia::any& val) override
  {
    if(const T* ptr = std::any_cast<T>(&val))
      s.stream() << *ptr;
    else
      cast_error("");
  }
  void apply(DataStream::Deserializer& s, ossia::any& val) override
  {
    T t;
    s.stream() >> t;
    val = std::move(t);
  }

  void apply(JSONObject::Serializer& s, const std::string& key, const ossia::any& val) override
  {
    if(const T* ptr = std::any_cast<T>(&val))
      s.obj[key] = *ptr;
    else
      cast_error("");
  }
  void apply(JSONObject::Deserializer& s, const std::string& key, ossia::any& val) override
  {
    T t;
    t <<= s.obj[key];
    val = std::move(t);
  }
};

using any_serializer_map = score::hash_map<std::string, std::unique_ptr<any_serializer>>;

//! The serializers for types that go in ossia::any should fit in here.
SCORE_LIB_BASE_EXPORT any_serializer_map& anySerializers();
}

inline void apply(DataStreamReader& s, const std::string& key, const ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if (it != ser.end())
  {
    it.value()->apply(s, v);
  }
  else
  {
    SCORE_TODO;
  }
}
inline void apply(DataStreamWriter& s, const std::string& key, ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if (it != ser.end())
  {
    it.value()->apply(s, v);
  }
  else
  {
    SCORE_TODO;
  }
}
inline void apply(JSONReader& s, const std::string& key, const ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if (it != ser.end())
  {
    it.value()->apply(s, key, v);
  }
  else
  {
    SCORE_TODO;
  }
}
inline void apply(JSONWriter& s, const std::string& key, ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if (it != ser.end())
  {
    it.value()->apply(s, key, v);
  }
  else
  {
    SCORE_TODO;
  }
}

template <>
struct SCORE_LIB_BASE_EXPORT TSerializer<DataStream, score::any_map>
{
  static void readFrom(DataStream::Serializer& s, const score::any_map& obj)
  {
    auto& st = s.stream();

    st << (int32_t)obj.size();
    for (const auto& e : obj)
    {
      st << e.first;
      apply(s, e.first, e.second);
    }
  }

  static void writeTo(DataStream::Deserializer& s, score::any_map& obj)
  {
    auto& st = s.stream();
    int32_t n;
    st >> n;
    for (int i = 0; i < n; i++)
    {
      std::string key;
      ossia::any value;
      st >> key;
      apply(s, key, value);
      obj.emplace(std::move(key), std::move(value));
    }
  }
};

template <>
struct SCORE_LIB_BASE_EXPORT TSerializer<JSONObject, score::any_map>
{
  static void readFrom(JSONObject::Serializer& s, const score::any_map& obj)
  {
    s.stream.StartObject();
    for (const auto& e : obj)
    {
      apply(s, e.first, e.second);
    }
    s.stream.EndObject();
  }

  static void writeTo(JSONObject::Deserializer& s, score::any_map& obj)
  {
    for (const auto& m : s.base.GetObject())
    {
      const std::string str(m.name.GetString(), m.name.GetStringLength());
      apply(s, str, obj[str]);
    }
  }
};

template <>
struct is_template<score::any_map> : std::true_type
{
};
