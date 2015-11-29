#include "CurvePointModel.hpp"
#include "iscore/tools/IdentifiedObject.hpp"

class QObject;

CurvePointModel::CurvePointModel(const Id<CurvePointModel>& id, QObject* parent):
    IdentifiedObject<CurvePointModel>{id, "CurvePointModel", parent}
{

}

const Id<CurveSegmentModel>& CurvePointModel::following() const
{
    return m_following;
}

void CurvePointModel::setFollowing(const Id<CurveSegmentModel> &following)
{
    m_following = following;
}

Curve::Point CurvePointModel::pos() const
{
    return m_pos;
}

void CurvePointModel::setPos(const Curve::Point &pos)
{
    m_pos = pos;
    emit posChanged();
}



const Id<CurveSegmentModel> &CurvePointModel::previous() const
{
    return m_previous;
}

void CurvePointModel::setPrevious(const Id<CurveSegmentModel> &previous)
{
    m_previous = previous;
}
