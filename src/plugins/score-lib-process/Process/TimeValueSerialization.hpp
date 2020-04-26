#pragma once
#include <Process/TimeValue.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QDebug>

inline QDebug operator<<(QDebug d, const TimeVal& tv)
{
  if (!tv.isInfinite())
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
    s.stream() << tv.isInfinite();

    if (!tv.isInfinite())
    {
      s.stream() << tv.msec();
    }
  }

  static void writeTo(DataStream::Deserializer& s, TimeVal& tv)
  {
    bool inf;
    s.stream() >> inf;

    if (!inf)
    {
      double msec;
      s.stream() >> msec;
      tv.setMSecs(msec);
    }
    else
    {
      tv = TimeVal::infinite();
    }
  }
};

template <>
struct TSerializer<JSONObject, TimeVal>
{
  static void readFrom(JSONObject::Serializer& s, const TimeVal& tv)
  {
    if (tv.isInfinite())
    {
      s.stream.String("inf");
    }
    else
    {
      s.stream.Double(tv.msec());
    }
  }

  static void writeTo(JSONObject::Deserializer& s, TimeVal& tv)
  {
    using namespace std;
    using namespace std::literals;
    if (s.base.IsString() && s.base.GetString() == "inf"sv)
    {
      tv = TimeVal::infinite();
    }
    else
    {
      tv.setMSecs(s.base.GetDouble());
    }
  }
};

