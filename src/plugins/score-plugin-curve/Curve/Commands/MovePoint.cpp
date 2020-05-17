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
MovePoint::MovePoint(const Model& curve, const Id<PointModel>& pointId, Curve::Point newPoint)
    : m_model{curve}, m_pointId{pointId}, m_newPoint{newPoint}
{
  for (auto& p : curve.points())
  {
    if (p->id() == m_pointId)
    {
      m_oldPoint = p->pos();
      break;
    }
  }
}

void MovePoint::undo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  for (auto& p : curve.points())
  {
    if (p->id() == m_pointId)
    {
      p->setPos(m_oldPoint);
      if (p->previous())
        curve.segments().at(*p->previous()).setEnd(m_oldPoint);
      if (p->following())
        curve.segments().at(*p->following()).setStart(m_oldPoint);
      break;
    }
  }
  curve.changed();
}

void MovePoint::redo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  for (auto& p : curve.points())
  {
    if (p->id() == m_pointId)
    {
      if (p->previous())
        curve.segments().at(*p->previous()).setEnd(m_newPoint);
      if (p->following())
        curve.segments().at(*p->following()).setStart(m_newPoint);
      p->setPos(m_newPoint);
      break;
    }
  }
  curve.changed();
}

void MovePoint::update(
    const Model& obj,
    const Id<PointModel>& pointId,
    const Curve::Point& newPoint)
{
  m_newPoint = newPoint;
}

void MovePoint::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_pointId << m_newPoint << m_oldPoint;
}

void MovePoint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_pointId >> m_newPoint >> m_oldPoint;
}
}
