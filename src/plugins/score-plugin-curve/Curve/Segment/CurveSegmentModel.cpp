// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSegmentModel.hpp"

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Curve::SegmentModel)
namespace Curve
{
SegmentModel::SegmentModel(const Id<SegmentModel>& id, QObject* parent)
    : IdentifiedObject<SegmentModel>{id, Metadata<ObjectKey_k, SegmentModel>::get(), parent}

{
}

SegmentModel::SegmentModel(const SegmentData& data, QObject* parent)
    : IdentifiedObject<SegmentModel>{data.id, Metadata<ObjectKey_k, SegmentModel>::get(), parent}
    , m_start{data.start}
    , m_end{data.end}
    , m_previous{data.previous}
    , m_following{data.following}
{
}

SegmentModel::SegmentModel(
    Curve::Point s,
    Curve::Point e,
    const Id<SegmentModel>& id,
    QObject* parent)
    : IdentifiedObject<SegmentModel>{id, Metadata<ObjectKey_k, SegmentModel>::get(), parent}
    , m_start{s}
    , m_end{e}
{
}

SegmentModel::SegmentModel(JSONObject::Deserializer& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

SegmentModel::SegmentModel(DataStream::Deserializer& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

SegmentModel::~SegmentModel() { }

void SegmentModel::setPrevious(const OptionalId<SegmentModel>& previous)
{
  if (previous != m_previous)
  {
    m_previous = previous;
    previousChanged();
  }
}

void SegmentModel::setVerticalParameter(double p) { }

void SegmentModel::setFollowing(const OptionalId<SegmentModel>& following)
{
  if (following != m_following)
  {
    m_following = following;
    followingChanged();
  }
}

void SegmentModel::setHorizontalParameter(double p) { }

std::optional<double> SegmentModel::verticalParameter() const
{
  return {};
}

std::optional<double> SegmentModel::horizontalParameter() const
{
  return {};
}

void SegmentModel::setStart(const Curve::Point& pt)
{
  if (pt != m_start)
  {
    m_start = pt;
    m_valid = false;
    on_startChanged();

    startChanged();
  }
}

void SegmentModel::setEnd(const Curve::Point& pt)
{
  if (pt != m_end)
  {
    m_end = pt;
    m_valid = false;
    on_endChanged();

    endChanged();
  }
}
}
