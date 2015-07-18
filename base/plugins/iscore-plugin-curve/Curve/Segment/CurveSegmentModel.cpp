#include "CurveSegmentModel.hpp"


CurveSegmentModel::CurveSegmentModel(const id_type<CurveSegmentModel>& id, QObject* parent):
    IdentifiedObject<CurveSegmentModel>{id, "CurveSegmentModel", parent}

{

}

CurveSegmentModel::~CurveSegmentModel()
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

void CurveSegmentModel::setVerticalParameter(double p)
{

}

void CurveSegmentModel::setFollowing(const id_type<CurveSegmentModel>& following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}

void CurveSegmentModel::setHorizontalParameter(double p)
{

}

boost::optional<double> CurveSegmentModel::verticalParameter() const
{
    return {};
}

boost::optional<double> CurveSegmentModel::horizontalParameter() const
{
    return {};
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
