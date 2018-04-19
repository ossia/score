#pragma once
#include "Value.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

SCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const ossia::value&);

inline QDataStream& operator<<(QDataStream& stream, const ossia::value& obj)
{
  DataStreamReader reader{stream.device()};
  reader.readFrom(obj);
  return stream;
}
inline DataStreamInput& operator<<(DataStreamInput& s, const ossia::value& obj)
{
  s.stream << obj;
  return s;
}

inline QDataStream& operator>>(QDataStream& stream, ossia::value& obj)
{
  DataStreamWriter writer{stream.device()};
  writer.writeTo(obj);
  return stream;
}
inline DataStreamOutput& operator>>(DataStreamOutput& s, ossia::value& obj)
{
  s.stream >> obj;
  return s;
}

JSON_METADATA(ossia::value, "Value")
