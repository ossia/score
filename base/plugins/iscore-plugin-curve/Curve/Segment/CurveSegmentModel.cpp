#include "Curve/Palette/CurvePoint.hpp"
#include "Curve/Segment/CurveSegmentData.hpp"
#include "CurveSegmentModel.hpp"
#include "iscore/tools/IdentifiedObject.hpp"

class QObject;


CurveSegmentModel::CurveSegmentModel(
        const Id<CurveSegmentModel>& id,
        QObject* parent):
    IdentifiedObject<CurveSegmentModel>{id, "CurveSegmentModel", parent}

{

}

CurveSegmentModel::CurveSegmentModel(
        const CurveSegmentData& data,
        QObject* parent):
    IdentifiedObject<CurveSegmentModel>{data.id, "CurveSegmentModel", parent},
    m_start{data.start},
    m_end{data.end},
    m_previous{data.previous},
    m_following{data.following}
{
}

CurveSegmentModel::~CurveSegmentModel()
{

}



void CurveSegmentModel::setPrevious(const Id<CurveSegmentModel>& previous)
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

void CurveSegmentModel::setFollowing(const Id<CurveSegmentModel>& following)
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

void CurveSegmentModel::setStart(const Curve::Point& pt)
{
    if(pt != m_start)
    {
        m_start = pt;
        m_valid = false;
        on_startChanged();

        emit startChanged();
    }
}

void CurveSegmentModel::setEnd(const Curve::Point& pt)
{
    if(pt != m_end)
    {
        m_end = pt;
        m_valid = false;
        on_endChanged();

        emit endChanged();
    }
}
