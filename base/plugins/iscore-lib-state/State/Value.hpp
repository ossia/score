#pragma once
#include <ossia/editor/value/value.hpp>
#include <ossia/editor/value/impulse.hpp>
#include <QChar>
#include <QList>
#include <QString>
#include <algorithm>
#include <eggs/variant.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <ossia-qt/metatypes.hpp>
#include <iscore_lib_state_export.h>
#include <vector>

class DataStream;
class JSONObject;
class QDebug;

namespace State
{
using impulse = ossia::impulse;

using vec2f = ossia::vec2f;
using vec3f = ossia::vec3f;
using vec4f = ossia::vec4f;
using tuple_t = std::vector<ossia::value>;

using Value = ossia::value;
using OptionalValue = optional<ossia::value>;

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m);

}

Q_DECLARE_METATYPE(ossia::val_type)


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
