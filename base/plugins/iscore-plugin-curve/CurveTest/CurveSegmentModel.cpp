#include "CurveSegmentModel.hpp"


CurveSegmentModel::CurveSegmentModel(const id_type<CurveSegmentModel>& id, QObject* parent):
    IdentifiedObject<CurveSegmentModel>{id, "CurveSegmentModel", parent}

{

}

const id_type<CurveSegmentModel>& CurveSegmentModel::previous() const
{
    return m_previous;
}

void CurveSegmentModel::setPrevious(const id_type<CurveSegmentModel>& previous)
{
    if(previous != m_previous)
    {
        m_previous = previous;
        emit previousChanged();
    }
}
const id_type<CurveSegmentModel>& CurveSegmentModel::following() const
{
    return m_following;
}

void CurveSegmentModel::setFollowing(const id_type<CurveSegmentModel>& following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}

QPointF CurveSegmentModel::end() const
{
    return m_end;
}

void CurveSegmentModel::setEnd(const QPointF& pt)
{
    m_end = pt;
    on_endChanged();
}

QPointF CurveSegmentModel::start() const
{
    return m_start;
}

void CurveSegmentModel::setStart(const QPointF& pt)
{
    m_start = pt;
    on_startChanged();
}
