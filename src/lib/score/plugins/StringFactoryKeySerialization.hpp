#pragma once
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
template <typename U>
struct TSerializer<DataStream, StringKey<U>>
{
  static void readFrom(DataStream::Serializer& s, const StringKey<U>& key)
  {
    s.stream() << key.toString();
  }

  static void writeTo(DataStream::Deserializer& s, StringKey<U>& key)
  {
    s.stream() >> key.toString();
  }
};

/*
template <typename U>
struct TSerializer<JSONValue, StringKey<U>>
{
  static void readFrom(JSONObject::Serializer& s, const StringKey<U>& key)
  {
    s.val = QString::fromStdString(key.toString());
  }

  static void writeTo(JSONValue::Serializer& s, StringKey<U>& key)
  {
    key.toString() = s.val.toString().toStdString();
  }
};
*/
