// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>
#include <QVariant>
#include <algorithm>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>

#include "CurveSegmentList.hpp"
#include "CurveSegmentModel.hpp"
#include "CurveSegmentModelSerialization.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/Identifier.hpp>


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamReader::read(const Curve::SegmentData& segmt)
{
  m_stream << segmt.id << segmt.start << segmt.end << segmt.previous
           << segmt.following << segmt.type;

  auto& csl = components.interfaces<Curve::SegmentList>();
  auto segmt_fact = csl.get(segmt.type);

  ISCORE_ASSERT(segmt_fact);
  segmt_fact->serializeCurveSegmentData(
      segmt.specificSegmentData, this->toVariant());

  insertDelimiter();
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamWriter::write(Curve::SegmentData& segmt)
{
  m_stream >> segmt.id >> segmt.start >> segmt.end >> segmt.previous
      >> segmt.following >> segmt.type;

  auto& csl = components.interfaces<Curve::SegmentList>();
  auto segmt_fact = csl.get(segmt.type);
  ISCORE_ASSERT(segmt_fact);
  segmt.specificSegmentData
      = segmt_fact->makeCurveSegmentData(this->toVariant());

  checkDelimiter();
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamReader::read(const Curve::SegmentModel& segmt)
{
  // Save this class (this will be loaded by writeTo(*this) in
  // CurveSegmentModel ctor
  m_stream << segmt.previous() << segmt.following() << segmt.start()
           << segmt.end();
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
DataStreamWriter::write(Curve::SegmentModel& segmt)
{
  m_stream >> segmt.m_previous >> segmt.m_following >> segmt.m_start
      >> segmt.m_end;

  // Note : don't call setStart/setEnd here since they
  // call virtual methods and this may be called from
  // CurveSegmentModel's constructor.
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
JSONObjectReader::read(const Curve::SegmentModel& segmt)
{
  using namespace Curve;

  // Save this class (this will be loaded by writeTo(*this) in
  // CurveSegmentModel ctor
  obj[strings.Previous] = toJsonValue(segmt.previous());
  obj[strings.Following] = toJsonValue(segmt.following());
  obj[strings.Start] = toJsonValue(segmt.start());
  obj[strings.End] = toJsonValue(segmt.end());
}


template <>
ISCORE_PLUGIN_CURVE_EXPORT void
JSONObjectWriter::write(Curve::SegmentModel& segmt)
{
  using namespace Curve;
  segmt.m_previous
      = fromJsonValue<OptionalId<SegmentModel>>(obj[strings.Previous]);
  segmt.m_following
      = fromJsonValue<OptionalId<SegmentModel>>(obj[strings.Following]);
  segmt.m_start = fromJsonValue<Curve::Point>(obj[strings.Start]);
  segmt.m_end = fromJsonValue<Curve::Point>(obj[strings.End]);
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
