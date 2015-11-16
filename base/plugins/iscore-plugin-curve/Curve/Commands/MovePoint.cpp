#include "MovePoint.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"


MovePoint::MovePoint(Path<CurveModel>&& model,
		     const Id<CurvePointModel>& pointId,
		     const CurvePoint& newPoint):
    iscore::SerializableCommand{
	 factoryName(), commandName(), description()},
    m_model{std::move(model)},
    m_pointId{pointId},
    m_newPoint{newPoint}
{

}

void MovePoint::undo() const
{

}

void MovePoint::redo() const
{

}

void MovePoint::update(Path<CurveModel>&& model, const Id<CurvePointModel>& pointId, const CurvePoint& newPoint)
{

}

void MovePoint::serializeImpl(QDataStream& s) const
{

}

void MovePoint::deserializeImpl(QDataStream& s)
{

}

const CurvePointModel&MovePoint::cpm()
{
/*    const auto& curve = m_model.find();
    for(auto& p : curve.points())
    {
        if(p->id() == m_pointId)
        {
            return p;
        }
    }
    */
}
