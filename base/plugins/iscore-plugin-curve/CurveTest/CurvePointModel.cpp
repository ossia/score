#include "CurvePointModel.hpp"


const id_type<CurveSegmentModel>& CurvePointModel::following() const
{
    return m_following;
}

void CurvePointModel::setFollowing(const id_type<CurveSegmentModel> &following)
{
    m_following = following;
}

const id_type<CurveSegmentModel> &CurvePointModel::previous() const
{
    return m_previous;
}

void CurvePointModel::setPrevious(const id_type<CurveSegmentModel> &previous)
{
    m_previous = previous;
}
