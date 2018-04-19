#pragma once
#include <ossia/detail/any_map.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/String.hpp>

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
  virtual void apply(JSONValue::Serializer&, const ossia::any&) = 0;
  virtual void apply(DataStream::Deserializer&, ossia::any&) = 0;
  virtual void apply(JSONValue::Deserializer&, ossia::any&) = 0;
};

template <typename T>
struct any_serializer_t final : public any_serializer
{
  void apply(DataStream::Serializer& s, const ossia::any& val) override
  {
    s.stream() << ossia::any_cast<T>(val);
  }
  void apply(JSONValue::Serializer& s, const ossia::any& val) override
  {
    s.val = toJsonValue(ossia::any_cast<T>(val));
  }
  void apply(DataStream::Deserializer& s, ossia::any& val) override
  {
    T t;
    s.stream() >> t;
    val = std::move(t);
  }
  void apply(JSONValue::Deserializer& s, ossia::any& val) override
  {
    val = fromJsonValue<T>(s.val);
  }
};

using any_serializer_map
    = score::hash_map<std::string, std::unique_ptr<any_serializer>>;

//! The serializers for types that go in ossia::any should fit in here.
SCORE_LIB_BASE_EXPORT any_serializer_map& anySerializers();
}

template <typename Vis, typename Any>
void apply(Vis& s, const std::string& key, Any& v)
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
    for (const auto& e : obj)
    {
      JSONValue::Serializer v{};
      apply(v, e.first, e.second);
      s.obj[QString::fromStdString(e.first)] = v.val;
    }
  }

  static void writeTo(JSONObject::Deserializer& s, score::any_map& obj)
  {
    const QJsonObject& extended = s.obj;
    auto end = extended.constEnd();
    for (auto it = extended.constBegin(); it != end; ++it)
    {
      JSONValue::Deserializer v{it.value()};
      auto key = it.key().toStdString();
      ossia::any val;
      apply(v, key, val);
      obj[std::move(key)] = std::move(val);
    }
  }
};

template <>
struct is_template<score::any_map> : std::true_type
{
};
