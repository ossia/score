#include "MovePoint.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"


MovePoint::MovePoint(Path<CurveModel>&& model,
             const Id<CurvePointModel>& pointId,
             CurvePoint newPoint):
    iscore::SerializableCommand{
	 factoryName(), commandName(), description()},
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
            curve.segments().at(p->previous()).setEnd(m_oldPoint);
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
            curve.segments().at(p->previous()).setEnd(m_newPoint);
            curve.segments().at(p->following()).setStart(m_newPoint);
            p->setPos(m_newPoint);
            break;
        }
    }
    curve.changed();
}

void MovePoint::update(Path<CurveModel>&& model, const Id<CurvePointModel>& pointId, const CurvePoint& newPoint)
{
    m_newPoint = newPoint;
}

void MovePoint::serializeImpl(QDataStream& s) const
{
    s << m_model << m_pointId << m_newPoint << m_oldPoint;
}

void MovePoint::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_pointId >> m_newPoint >> m_oldPoint;
}
