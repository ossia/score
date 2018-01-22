#pragma once
#include <Process/TimeValue.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
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
      tv = TimeVal{PositiveInfinity{}};
    }
  }
};


template <>
struct TSerializer<JSONValue, TimeVal>
{
  static void readFrom(JSONValue::Serializer& s, const TimeVal& tv)
  {
    if (tv.isInfinite())
    {
      s.val = "inf";
    }
    else
    {
      s.val = tv.msec();
    }
  }

  static void writeTo(JSONValue::Deserializer& s, TimeVal& tv)
  {
    if (s.val.toString() == "inf")
    {
      tv = TimeVal{PositiveInfinity{}};
    }
    else
    {
      tv.setMSecs(s.val.toDouble());
    }
  }
};
