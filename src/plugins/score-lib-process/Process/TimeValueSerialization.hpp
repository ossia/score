#pragma once
#include <Process/TimeValue.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QDebug>

inline QDebug operator<<(QDebug d, const TimeVal& tv)
{
  if (!tv.infinite())
  {
    d << tv.msec() << "ms";
  }
  else
  {
    d << "infinite";
  }

  return d;
}

template <>
struct TSerializer<DataStream, TimeVal>
{
  static void readFrom(DataStream::Serializer& s, const TimeVal& tv)
  {
    s.stream() << tv.impl;
  }

  static void writeTo(DataStream::Deserializer& s, TimeVal& tv)
  {
    s.stream() >> tv.impl;
  }
};

template <>
struct TSerializer<JSONObject, TimeVal>
{
  static void readFrom(JSONObject::Serializer& s, const TimeVal& tv)
  {
    s.stream.Int64(tv.impl);
  }

  static void writeTo(JSONObject::Deserializer& s, TimeVal& tv)
  {
    using namespace std;
    using namespace std::literals;
    tv.impl = s.base.GetInt64();
  }
};

