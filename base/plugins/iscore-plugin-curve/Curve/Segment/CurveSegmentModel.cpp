#include "CurveSegmentModel.hpp"


CurveSegmentModel::CurveSegmentModel(const id_type<CurveSegmentModel>& id, QObject* parent):
    IdentifiedObject<CurveSegmentModel>{id, "CurveSegmentModel", parent}

{

}



void CurveSegmentModel::setPrevious(const id_type<CurveSegmentModel>& previous)
{
    if(previous != m_previous)
    {
        m_previous = previous;
        emit previousChanged();
    }
}


void CurveSegmentModel::setFollowing(const id_type<CurveSegmentModel>& following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}



void CurveSegmentModel::setEnd(const CurvePoint& pt)
{
    if(pt != m_end)
    {
        m_end = pt;
        m_valid = false;
        on_endChanged();
    }
}



void CurveSegmentModel::setStart(const CurvePoint& pt)
{
    if(pt != m_start)
    {
        m_start = pt;
        m_valid = false;
        on_startChanged();
    }
}
