#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <boost/any.hpp>

/**
 * \file AnySerialization.hpp
 * \brief Serialization mechanism for boost::any
 *
 * This file provides a method to save data registered in a boost::any.
 * By default some types will be serialized / deserialized without problems.
 *
 * For the others, one should register them in anySerializers()
 *
 *
 * \todo See the one used in boost.spirit which is said to be much faster.
 */
namespace iscore
{
using any_map = iscore::hash_map<std::string, boost::any>;

struct ISCORE_LIB_BASE_EXPORT any_serializer
{
  virtual ~any_serializer();
  virtual void apply(Serializer<DataStream>&, const boost::any&) = 0;
  virtual void apply(Serializer<JSONValue>&, const boost::any&) = 0;
  virtual void apply(Deserializer<DataStream>&, boost::any&) = 0;
  virtual void apply(Deserializer<JSONValue>&, boost::any&) = 0;
};

template<typename T>
struct any_serializer_t final : public any_serializer
{
  void apply(Serializer<DataStream>& s, const boost::any& val) override
  { s.stream() << boost::any_cast<T>(val); }
  void apply(Serializer<JSONValue>& s, const boost::any& val) override
  { s.val = toJsonValue(boost::any_cast<T>(val)); }
  void apply(Deserializer<DataStream>& s, boost::any& val) override
  {
    T t;
    s.stream() >> t;
    val = std::move(t);
  }
  void apply(Deserializer<JSONValue>& s, boost::any& val) override
  {
    val = fromJsonValue<T>(s.val);
  }
};

using any_serializer_map =
  iscore::hash_map<std::string, std::unique_ptr<any_serializer>>;

//! The serializers for types that go in boost::any should fit in here.
ISCORE_LIB_BASE_EXPORT any_serializer_map& anySerializers();
}

template<typename Vis, typename Any>
void apply(Vis& s, const std::string& key, Any& v)
{
  const auto& t = v.type();
  if(t == typeid(int32_t))
    iscore::any_serializer_t<int32_t>{}.apply(s, v);
  else if(t == typeid(std::string))
    iscore::any_serializer_t<std::string>{}.apply(s, v);
  else if(t == typeid(std::vector<std::string>))
    iscore::any_serializer_t<int32_t>{}.apply(s, v);
  else
  {
    auto& ser = iscore::anySerializers();
    auto it = ser.find(key);
    if(it != ser.end())
    {
      it.value()->apply(s, v);
    }
    else
    {
      ISCORE_TODO;
    }
  }
}

template<>
struct ISCORE_LIB_BASE_EXPORT TSerializer<DataStream, void, iscore::any_map>
{
  static void readFrom(
      DataStream::Serializer& s,
      const iscore::any_map& obj)
  {
    auto& st = s.stream();

    st << (int32_t) obj.size();
    for(const auto& e : obj)
    {
      st << e.first;
      apply(s, e.first, e.second);
    }
  }

  static void writeTo(
      DataStream::Deserializer& s,
      iscore::any_map& obj)
  {
    auto& st = s.stream();
    int32_t n;
    st >> n;
    for(int i = 0; i < n; i++)
    {
      std::string key;
      boost::any value;
      st >> key;
      apply(s, key, value);
      obj.emplace(std::move(key), std::move(value));
    }
  }
};


template<>
struct ISCORE_LIB_BASE_EXPORT TSerializer<JSONObject, iscore::any_map>
{
  static void readFrom(
      JSONObject::Serializer& s,
      const iscore::any_map& obj)
  {
    for(const auto& e : obj)
    {
      Serializer<JSONValue> v{};
      apply(v, e.first, e.second);
      s.m_obj[QString::fromStdString(e.first)] = v.val;
    }
  }


  static void writeTo(
      JSONObject::Deserializer& s,
      iscore::any_map& obj)
  {
    const QJsonObject& extended = s.m_obj;
    for(const auto& k : extended.keys())
    {
      Deserializer<JSONValue> v{extended[k]};
      auto key = k.toStdString();
      boost::any val;
      apply(v, key, val);
      obj[std::move(key)] = std::move(val);
    }
  }
};

template <>
struct is_template<iscore::any_map> : std::true_type
{
};
