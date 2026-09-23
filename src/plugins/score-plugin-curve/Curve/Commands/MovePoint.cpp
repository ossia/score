// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "MovePoint.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Curve
{
namespace
{
PointModel*
findPoint(const Model& curve, const OptionalId<SegmentModel>& prev, const OptionalId<SegmentModel>& foll)
{
  for(auto p : curve.points())
    if(p->previous() == prev && p->following() == foll)
      return p;
  return nullptr;
}

void setPoint(Model& curve, PointModel& p, Curve::Point pos)
{
  auto& segs = curve.segments();
  if(p.previous())
    if(auto it = segs.find(*p.previous()); it != segs.end())
      it->setEnd(pos);
  if(p.following())
    if(auto it = segs.find(*p.following()); it != segs.end())
      it->setStart(pos);
  p.setPos(pos);
  curve.changed();
}
}

MovePoint::MovePoint(
    const Model& curve, const Id<PointModel>& pointId, Curve::Point newPoint)
    : m_model{curve}
    , m_newPoint{newPoint}
{
  for(auto& p : curve.points())
  {
    if(p->id() == pointId)
    {
      m_previous = p->previous();
      m_following = p->following();
      m_oldPoint = p->pos();

      // Otherwise sometimes we have the case where we loose precision in
      // inspector edition: the point ends up moving past the next one
      // if e.g. drawing a square wave and changing a point on the same X tha n
      // another
      if(std::abs(m_oldPoint.x() - m_newPoint.x()) < 0.0001)
      {
        m_newPoint.rx() = m_oldPoint.x();
      }

      break;
    }
  }
}

void MovePoint::undo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  if(auto p = findPoint(curve, m_previous, m_following))
    setPoint(curve, *p, m_oldPoint);
}

void MovePoint::redo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  if(auto p = findPoint(curve, m_previous, m_following))
    setPoint(curve, *p, m_newPoint);
}

void MovePoint::update(
    const Model& obj, const Id<PointModel>& pointId, const Curve::Point& newPoint)
{
  m_newPoint = newPoint;
}

void MovePoint::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_previous << m_following << m_newPoint << m_oldPoint;
}

void MovePoint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_previous >> m_following >> m_newPoint >> m_oldPoint;
}
}
