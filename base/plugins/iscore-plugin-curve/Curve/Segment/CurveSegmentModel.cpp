#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "CurveSegmentModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;

namespace Curve
{
SegmentModel::SegmentModel(
        const Id<SegmentModel>& id,
        QObject* parent):
    IdentifiedObject<SegmentModel>{id, "CurveSegmentModel", parent}

{

}

SegmentModel::SegmentModel(
        const SegmentData& data,
        QObject* parent):
    IdentifiedObject<SegmentModel>{data.id, "CurveSegmentModel", parent},
    m_start{data.start},
    m_end{data.end},
    m_previous{data.previous},
    m_following{data.following}
{
}

SegmentModel::~SegmentModel()
{

}



void SegmentModel::setPrevious(const Id<SegmentModel>& previous)
{
    if(previous != m_previous)
    {
        m_previous = previous;
        emit previousChanged();
    }
}

void SegmentModel::setVerticalParameter(double p)
{

}

void SegmentModel::setFollowing(const Id<SegmentModel>& following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}

void SegmentModel::setHorizontalParameter(double p)
{

}

boost::optional<double> SegmentModel::verticalParameter() const
{
    return {};
}

boost::optional<double> SegmentModel::horizontalParameter() const
{
    return {};
}

void SegmentModel::setStart(const Curve::Point& pt)
{
    if(pt != m_start)
    {
        m_start = pt;
        m_valid = false;
        on_startChanged();

        emit startChanged();
    }
}

void SegmentModel::setEnd(const Curve::Point& pt)
{
    if(pt != m_end)
    {
        m_end = pt;
        m_valid = false;
        on_endChanged();

        emit endChanged();
    }
}
}
