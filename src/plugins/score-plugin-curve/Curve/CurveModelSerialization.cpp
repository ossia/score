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
static_assert(is_template<IdContainer<Curve::SegmentModel>>::value);
static_assert(std::is_same_v<
              serialization_tag<IdContainer<Curve::SegmentModel>>::type,
              visitor_template_tag>);
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
  for(const auto& seg : segments)
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

  curve.segments().m_map.reserve(size);
  curve.points().reserve(size + 2);

  static std::vector<Curve::SegmentModel*> segts;
  segts.clear();
  segts.reserve(size);

  auto& csl = components.interfaces<Curve::SegmentList>();
  for(; size-- > 0;)
  {
    auto seg = deserialize_interface(csl, *this, &curve);
    if(seg)
      segts.push_back(seg);
    else
      SCORE_TODO;
  }
  std::sort(
      segts.begin(), segts.end(), [](Curve::SegmentModel* a, Curve::SegmentModel* b) {
    return a->m_start.x() < b->m_start.x();
  });

  curve.loadSegments(segts);
  segts.clear();
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
  curve.segments().m_map.reserve(segments.Size());
  curve.points().reserve(segments.Size() + 2);

  static std::vector<Curve::SegmentModel*> segts;
  segts.clear();
  segts.reserve(segments.Size());

  for(const auto& segment : segments)
  {
    JSONObject::Deserializer segment_deser{segment};
    auto seg = deserialize_interface(csl, segment_deser, &curve);
    if(seg)
      segts.push_back(seg);
    else
      SCORE_TODO;
  }

  std::sort(
      segts.begin(), segts.end(), [](Curve::SegmentModel* a, Curve::SegmentModel* b) {
    return a->m_start.x() < b->m_start.x();
  });
  curve.loadSegments(segts);
  segts.clear();

  curve.changed();
}
