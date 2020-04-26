// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSegmentModelSerialization.hpp"

#include "CurveSegmentList.hpp"
#include "CurveSegmentModel.hpp"
#include "CurveSegmentView.hpp"

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


#include <vector>

#if !defined(SCORE_ALL_UNITY)
// template class SCORE_PLUGIN_CURVE_EXPORT IdContainer<Curve::SegmentModel>;
template class SCORE_PLUGIN_CURVE_EXPORT
    IdContainer<Curve::PointView, Curve::PointModel>;
template class SCORE_PLUGIN_CURVE_EXPORT
    IdContainer<Curve::SegmentView, Curve::SegmentModel>;
#endif

template <>
SCORE_PLUGIN_CURVE_EXPORT void
DataStreamReader::read(const Curve::SegmentData& segmt)
{
  m_stream << segmt.id << segmt.start << segmt.end << segmt.previous
           << segmt.following << segmt.type;

  auto& csl = components.interfaces<Curve::SegmentList>();
  auto segmt_fact = csl.get(segmt.type);

  SCORE_ASSERT(segmt_fact);
  segmt_fact->serializeCurveSegmentData(
      segmt.specificSegmentData, this->toVariant());

  insertDelimiter();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void
DataStreamWriter::write(Curve::SegmentData& segmt)
{
  m_stream >> segmt.id >> segmt.start >> segmt.end >> segmt.previous
      >> segmt.following >> segmt.type;

  auto& csl = components.interfaces<Curve::SegmentList>();
  auto segmt_fact = csl.get(segmt.type);
  SCORE_ASSERT(segmt_fact);
  segmt.specificSegmentData
      = segmt_fact->makeCurveSegmentData(this->toVariant());

  checkDelimiter();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void
DataStreamReader::read(const Curve::SegmentModel& segmt)
{
  // Save this class (this will be loaded by writeTo(*this) in
  // CurveSegmentModel ctor
  m_stream << segmt.previous() << segmt.following() << segmt.start()
           << segmt.end();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void
DataStreamWriter::write(Curve::SegmentModel& segmt)
{
  m_stream >> segmt.m_previous >> segmt.m_following >> segmt.m_start
      >> segmt.m_end;

  // Note : don't call setStart/setEnd here since they
  // call virtual methods and this may be called from
  // CurveSegmentModel's constructor.
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void
JSONReader::read(const Curve::SegmentModel& segmt)
{
  using namespace Curve;

  // Save this class (this will be loaded by writeTo(*this) in
  // CurveSegmentModel ctor
  obj[strings.Previous] = segmt.previous();
  obj[strings.Following] = segmt.following();
  obj[strings.Start] = segmt.start();
  obj[strings.End] = segmt.end();
}

template <>
SCORE_PLUGIN_CURVE_EXPORT void
JSONWriter::write(Curve::SegmentModel& segmt)
{
  using namespace Curve;
  segmt.m_previous <<= obj[strings.Previous];
  segmt.m_following <<= obj[strings.Following];
  segmt.m_start <<= obj[strings.Start];
  segmt.m_end <<= obj[strings.End];
}

namespace Curve
{
Curve::SegmentModel* createCurveSegment(
    const Curve::SegmentList& csl,
    const Curve::SegmentData& dat,
    QObject* parent)
{
  auto fact = csl.get(dat.type);
  auto model = fact->load(dat, parent);

  return model;
}
}
