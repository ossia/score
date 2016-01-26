#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>
#include <boost/optional/optional.hpp>

#include <iscore/serialization/VisitorCommon.hpp>
#include <QDebug>
#include <cstddef>
#include <functional>
#include <iterator>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "PointArrayCurveSegmentModel.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include "psimpl.h"

class QObject;
namespace Curve
{
PointArraySegment::PointArraySegment(
        const SegmentData& dat,
        QObject* parent):
    SegmentModel{dat, parent}
{
    const auto& pa_data = dat.specificSegmentData.value<PointArraySegmentData>();
    min_x = pa_data.min_x;
    max_x = pa_data.max_x;
    min_y = pa_data.min_y;
    max_y = pa_data.max_y;

    for(auto pt : pa_data.m_points)
    {
        m_points.insert(std::make_pair(pt.x(), pt.y()));
    }
}


SegmentModel*PointArraySegment::clone(
        const Id<SegmentModel>& id,
        QObject *parent) const
{
    auto cs = new PointArraySegment{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

const SegmentFactoryKey& PointArraySegment::key() const
{
    static const SegmentFactoryKey name{"c598b840-db67-4c8f-937a-46cfac87cb59"};
    return name;
}

void PointArraySegment::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void PointArraySegment::on_startChanged()
{
    emit dataChanged();
}

void PointArraySegment::on_endChanged()
{
    emit dataChanged();
}

void PointArraySegment::updateData(int numInterp) const
{
    if(!m_valid)
    {
        m_data.clear();
        m_data.reserve(m_points.size());

        double length = max_x - min_x;
        double amplitude = max_y - min_y;
        // Scale all the points between 0 / 1 in <->
        // and the local min / max in vertical

        for(const auto& elt : m_points)
        {
            m_data.push_back(
            { (m_end.x() - m_start.x()) * (elt.first - min_x) / length + m_start.x(),
              (elt.second - min_y) / amplitude });
        }
    }
}

double PointArraySegment::valueAt(double x) const
{
    ISCORE_TODO;
    return 0;
}

void PointArraySegment::addPoint(double x, double y)
{
    // If x < start.x() or x > end.x(), we update start / end
    // The points must keep their apparent position.
    // If y < 0 or y > 1, we rescale everything (and update min / max)
    int s = m_points.size();

    if(y < min_y)
        min_y = y;
    if(y > max_y)
        max_y = y;

    if(s > 0)
    {
        if(x < min_x)
            min_x = x;
        else if(x > max_x)
            max_x = x;

        if(y < min_y)
            min_y = y;
        else if(y > max_y)
            max_y = y;
    }
    else
    {
        min_x = x;
        max_x = x;
        min_y = y;
        max_y = y;
    }

    m_points.insert(std::make_pair(x, y));

    m_valid = false;
    emit dataChanged();
}

void PointArraySegment::simplify()
{
    double tolerance = (max_y - min_y) / 10.;

    std::vector <double> orig;
    orig.reserve(m_points.size() * 2);
    for(const auto& pt : m_points)
    {
        orig.push_back(pt.first);
        orig.push_back(pt.second);
    }

    std::vector <double> result;
    result.reserve(m_points.size() / 2);

    psimpl::simplify_reumann_witkam <2> (
        orig.begin (), orig.end (),
        tolerance, std::back_inserter (result));
    ISCORE_ASSERT(result.size() > 0);
    ISCORE_ASSERT(result.size() % 2 == 0);

    m_points.clear();
    m_points.reserve(result.size() / 2);
    for(auto i = 0u; i < result.size(); i+= 2)
    {
        m_points.insert(std::make_pair(result[i], result[i+1]));
    }
}

std::vector<std::unique_ptr<LinearSegment> > PointArraySegment::piecewise() const
{
    m_valid = false;
    updateData(0);
    const auto& pts = data();
    std::vector<std::unique_ptr<LinearSegment> > vec;
    vec.reserve(pts.size() - 1);


    for(std::size_t i = 0; i < pts.size() - 1; i++)
    {
        auto cmd = std::make_unique<LinearSegment>(Id<SegmentModel>(i), nullptr);
        cmd->setStart(pts[i]);
        cmd->setEnd(pts[i+1]);
        if(i > 0)
        {
            cmd->setPrevious(Id<SegmentModel>(i-1));
            vec.back()->setFollowing(Id<SegmentModel>(i));
        }
        vec.push_back(std::move(cmd));

    }

    return vec;
}

std::vector<SegmentData> PointArraySegment::toLinearSegments() const
{
    m_valid = false;
    updateData(0);
    const auto& pts = data();
    std::vector<SegmentData> vec;
    vec.reserve(pts.size() - 1);

    vec.emplace_back(Id<SegmentModel>{0},
                     pts[0], pts[1],
                     Id<SegmentModel>{}, Id<SegmentModel>{},
                     LinearSegmentData::key(), QVariant::fromValue(LinearSegmentData{}));

    int size = pts.size();
    for(int i = 1; i < size - 1; i++)
    {
        vec.back().following = Id<SegmentModel>{i};

        vec.emplace_back(Id<SegmentModel>{i},
                         pts[i], pts[i+1],
                         Id<SegmentModel>{i-1}, Id<SegmentModel>{},
                         LinearSegmentData::key(), QVariant::fromValue(LinearSegmentData()));
    }

    return vec;
}
}
