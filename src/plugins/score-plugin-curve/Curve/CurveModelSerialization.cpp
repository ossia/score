// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveModel.hpp"

#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <sys/types.h>

template <>
SCORE_PLUGIN_CURVE_EXPORT void DataStreamReader::read(const Curve::CurveDomain& dom)
{
  m_stream << dom.min << dom.max << dom.start << dom.end;
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void DataStreamWriter::write(Curve::CurveDomain& dom)
{
  m_stream >> dom.min >> dom.max >> dom.start >> dom.end;
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void DataStreamReader::read(const Curve::Model& curve)
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
SCORE_PLUGIN_CURVE_EXPORT void DataStreamWriter::write(Curve::Model& curve)
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
      SCORE_TODO;
  }

  curve.changed();
  checkDelimiter();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void JSONReader::read(const Curve::Model& curve)
{
  obj[strings.Segments] = curve.segments();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void JSONWriter::write(Curve::Model& curve)
{
  auto& csl = components.interfaces<Curve::SegmentList>();
  const auto& segments = obj[strings.Segments].toArray();
  for (const auto& segment : segments)
  {
    JSONObject::Deserializer segment_deser{segment};
    auto seg = deserialize_interface(csl, segment_deser, &curve);
    if (seg)
      curve.addSegment(seg);
    else
      SCORE_TODO;
  }

  curve.changed();
}
