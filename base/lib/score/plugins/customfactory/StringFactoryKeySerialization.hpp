#pragma once
#include <score/plugins/customfactory/StringFactoryKey.hpp>

template <typename U>
struct TSerializer<JSONValue, StringKey<U>>
{
  static void readFrom(JSONValue::Serializer& s, const StringKey<U>& key)
  {
    s.val = QString::fromStdString(key.toString());
  }

  static void writeTo(JSONValue::Serializer& s, StringKey<U>& key)
  {
    key.toString() = s.val.toString().toStdString();
  }
};
