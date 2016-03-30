
#include <QPoint>
#include <algorithm>

#include <Curve/CurveModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include "MovePoint.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Curve
{
MovePoint::MovePoint(Path<Model>&& model,
                     const Id<PointModel>& pointId,
                     Curve::Point newPoint):
    m_model{std::move(model)},
    m_pointId{pointId},
    m_newPoint{newPoint}
{
    auto& curve = m_model.find();
    for(auto& p : curve.points())
    {
        if(p->id() == m_pointId)
        {
            m_oldPoint = p->pos();
            break;
        }
    }
}

void MovePoint::undo() const
{
    auto& curve = m_model.find();
    for(auto& p : curve.points())
    {
        if(p->id() == m_pointId)
        {
            p->setPos(m_oldPoint);
            if(p->previous())
                curve.segments().at(p->previous()).setEnd(m_oldPoint);
            if(p->following())
                curve.segments().at(p->following()).setStart(m_oldPoint);
            break;
        }
    }
    curve.changed();
}

void MovePoint::redo() const
{
    auto& curve = m_model.find();
    for(auto& p : curve.points())
    {
        if(p->id() == m_pointId)
        {
            if(p->previous())
                curve.segments().at(p->previous()).setEnd(m_newPoint);
            if(p->following())
                curve.segments().at(p->following()).setStart(m_newPoint);
            p->setPos(m_newPoint);
            break;
        }
    }
    curve.changed();
}

void MovePoint::update(
        Path<Model>&& model,
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
