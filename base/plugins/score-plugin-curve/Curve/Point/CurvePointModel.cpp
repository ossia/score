// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePointModel.hpp"
#include <score/model/IdentifiedObject.hpp>

class QObject;

namespace Curve
{
PointModel::PointModel(const Id<PointModel>& id, QObject* parent)
    : IdentifiedObject<PointModel>{id, QStringLiteral("CurvePointModel"), parent}
{
}

const OptionalId<SegmentModel>& PointModel::following() const
{
  return m_following;
}

void PointModel::setFollowing(const OptionalId<SegmentModel>& following)
{
  m_following = following;
}

Curve::Point PointModel::pos() const
{
  return m_pos;
}

void PointModel::setPos(const Curve::Point& pos)
{
  m_pos = pos;
  posChanged();
}

const OptionalId<SegmentModel>& PointModel::previous() const
{
  return m_previous;
}

void PointModel::setPrevious(const OptionalId<SegmentModel>& previous)
{
  m_previous = previous;
}
}
