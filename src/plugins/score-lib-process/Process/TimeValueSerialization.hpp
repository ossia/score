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
  static void readFrom(DataStream::Serializer& s, const TimeVal& tv) { s.stream() << tv.impl; }

  static void writeTo(DataStream::Deserializer& s, TimeVal& tv) { s.stream() >> tv.impl; }
};

template <>
struct TSerializer<JSONObject, TimeVal>
{
  static void readFrom(JSONObject::Serializer& s, const TimeVal& tv)
  {
    if(Q_UNLIKELY(tv.impl > ossia::time_value::infinite_min))
    {
      s.stream.Int64(ossia::time_value::infinity);
    }
    else if(Q_UNLIKELY(tv.impl < 0))
    {
      qDebug() << "Warning: saving a time_value < 0. This likely indicates a bug: " << tv.impl;
      s.stream.Int64(0);
    }
    else
    {
      s.stream.Int64(tv.impl);
    }
  }

  static void writeTo(JSONObject::Deserializer& s, TimeVal& tv)
  {
    using namespace std;
    using namespace std::literals;
    if(Q_LIKELY(s.base.IsInt64()))
    {
      tv.impl = s.base.GetInt64();
    }
    else if(s.base.IsUint64())
    {
      // Note: there is likely a rapidjson bug there...
      qDebug() << "Warning: loading a value > to the maximum of an int64_t: " << s.base.GetUint64();
      tv.impl = ossia::time_value::infinity;
    }
    else
    {
      qDebug() << "Warning: could not load a TimeVal";
      tv.impl = 0;
    }
  }
};
