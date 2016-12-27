#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <sys/types.h>

#include "CurveModel.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamReader::read(const Curve::Model& curve)
{
  const auto& segments = curve.segments();

  m_stream << (int32_t)segments.size();
  for (const auto& seg : segments)
  {
    readFrom(seg);
  }
  insertDelimiter();
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamWriter::writeTo(Curve::Model& curve)
{
  int32_t size;
  m_stream >> size;

  auto& csl = components.interfaces<Curve::SegmentList>();
  for (; size-- > 0;)
  {
    auto seg = deserialize_interface(csl, *this, &curve);
    if (seg)
      curve.addSegment(seg);
    else
      ISCORE_TODO;
  }

  curve.changed();
  checkDelimiter();
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
JSONObjectReader::read(const Curve::Model& curve)
{
  obj["Segments"] = toJsonArray(curve.segments());
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
JSONObjectWriter::writeTo(Curve::Model& curve)
{
  auto& csl = components.interfaces<Curve::SegmentList>();
  for (const auto& segment : obj["Segments"].toArray())
  {
    JSONObject::Deserializer segment_deser{segment.toObject()};
    auto seg = deserialize_interface(csl, segment_deser, &curve);
    if (seg)
      curve.addSegment(seg);
    else
      ISCORE_TODO;
  }

  curve.changed();
}
