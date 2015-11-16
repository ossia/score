#include "PointArrayCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include "psimpl.h"

PointArrayCurveSegmentModel::PointArrayCurveSegmentModel(
        const CurveSegmentData& dat,
        QObject* parent):
    CurveSegmentModel{dat, parent}
{
    const auto& pa_data = dat.specificSegmentData.value<PointArrayCurveSegmentData>();
    min_x = pa_data.min_x;
    max_x = pa_data.max_x;
    min_y = pa_data.min_y;
    max_y = pa_data.max_y;

    for(auto pt : pa_data.m_points)
    {
        m_points.insert(std::make_pair(pt.x(), pt.y()));
    }
}


CurveSegmentModel*PointArrayCurveSegmentModel::clone(
        const Id<CurveSegmentModel>& id,
        QObject *parent) const
{
    auto cs = new PointArrayCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

QString PointArrayCurveSegmentModel::name() const
{
    return "PointArray";
}

void PointArrayCurveSegmentModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void PointArrayCurveSegmentModel::on_startChanged()
{
    emit dataChanged();
}

void PointArrayCurveSegmentModel::on_endChanged()
{
    emit dataChanged();
}

void PointArrayCurveSegmentModel::updateData(int numInterp) const
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

double PointArrayCurveSegmentModel::valueAt(double x) const
{
    ISCORE_TODO;
    return 0;
}

void PointArrayCurveSegmentModel::addPoint(double x, double y)
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

void PointArrayCurveSegmentModel::simplify()
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

std::vector<std::unique_ptr<LinearCurveSegmentModel> > PointArrayCurveSegmentModel::piecewise() const
{
    m_valid = false;
    updateData(0);
    const auto& pts = data();
    std::vector<std::unique_ptr<LinearCurveSegmentModel> > vec;
    vec.reserve(pts.size() - 1);


    for(std::size_t i = 0; i < pts.size() - 1; i++)
    {
        auto cmd = std::make_unique<LinearCurveSegmentModel>(Id<CurveSegmentModel>(i), nullptr);
        cmd->setStart(pts[i]);
        cmd->setEnd(pts[i+1]);
        if(i > 0)
        {
            cmd->setPrevious(Id<CurveSegmentModel>(i-1));
            vec.back()->setFollowing(Id<CurveSegmentModel>(i));
        }
        vec.push_back(std::move(cmd));

    }

    return vec;
}

std::vector<CurveSegmentData> PointArrayCurveSegmentModel::toLinearSegments() const
{
    m_valid = false;
    updateData(0);
    const auto& pts = data();
    std::vector<CurveSegmentData> vec;
    vec.reserve(pts.size() - 1);

    vec.emplace_back(Id<CurveSegmentModel>{0},
                     pts[0], pts[1],
                     Id<CurveSegmentModel>{}, Id<CurveSegmentModel>{},
                     QString{"Linear"}, QVariant::fromValue(LinearCurveSegmentData{}));

    int size = pts.size();
    for(int i = 1; i < size - 1; i++)
    {
        vec.back().following = Id<CurveSegmentModel>{i};

        vec.emplace_back(Id<CurveSegmentModel>{i},
                         pts[i], pts[i+1],
                         Id<CurveSegmentModel>{i-1}, Id<CurveSegmentModel>{},
                         QString{"Linear"}, QVariant::fromValue(LinearCurveSegmentData()));
    }

    return vec;
}
