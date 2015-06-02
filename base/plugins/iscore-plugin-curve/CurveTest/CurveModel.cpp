#include "CurveModel.hpp"
#include "CurveSegmentModel.hpp"
#include "CurvePointModel.hpp"
CurveModel::CurveModel(const id_type<CurveModel>& id, QObject* parent):
    IdentifiedObject<CurveModel>(id, "CurveModel", parent)
{

}

void CurveModel::addSegment(CurveSegmentModel* m)
{
    m->setParent(this);
    m_segments.append(m);
    emit segmentAdded(m);

    // Add points if necessary
    // If there is an existing previous segment, its end point also exists
    if(m->previous())
    {
        auto previousSegment = std::find_if(
                    m_segments.begin(),
                    m_segments.end(),
                    [&] (CurveSegmentModel* seg) { return seg->following() == m->id(); });
        if(previousSegment != m_segments.end())
        {
            auto thePt = std::find_if(
                        m_points.begin(),
                        m_points.end(),
                        [&] (CurvePointModel* pt) { return pt->previous() == (*previousSegment)->id(); });

            if(thePt != m_points.end())
            {
                (*thePt)->setFollowing(m->id());
            }
            else
            {
                auto pt = new CurvePointModel{this};
                pt->setFollowing(m->id());
                addPoint(pt);
            }
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->following() == m->id(); }))
    {
        auto pt = new CurvePointModel{this};
        pt->setFollowing(m->id());
        addPoint(pt);
    }

    if(m->following())
    {
        auto followingSegment = std::find_if(
                    m_segments.begin(),
                    m_segments.end(),
                    [&] (CurveSegmentModel* seg) { return seg->previous() == m->id(); });
        if(followingSegment != m_segments.end())
        {
            auto thePt = std::find_if(
                        m_points.begin(),
                        m_points.end(),
                        [&] (CurvePointModel* pt) { return pt->following() == (*followingSegment)->id(); });

            if(thePt != m_points.end())
            {
                (*thePt)->setPrevious(m->id());
            }
            else
            {
                auto pt = new CurvePointModel{this};
                pt->setPrevious(m->id());
                addPoint(pt);
            }
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->previous() == m->id(); }))
    {
        auto pt = new CurvePointModel{this};
        pt->setPrevious(m->id());
        addPoint(pt);
    }
}


void CurveModel::removeSegment(CurveSegmentModel* m)
{
    auto index = m_segments.indexOf(m);
    if(index >= 0)
    {
        m_segments.remove(index);
    }

    emit segmentRemoved(m);

    for(CurvePointModel* pt : m_points)
    {
        if(pt->previous() == m->id())
        {
            pt->setPrevious(id_type<CurveSegmentModel>{});
        }

        if(pt->following() == m->id())
        {
            pt->setFollowing(id_type<CurveSegmentModel>{});
        }

        if(!pt->previous() && !pt->following())
        {
            removePoint(pt);
        }
    }

    delete m;
}

Selection CurveModel::selectedChildren() const
{
    Selection s;
    std::copy_if(m_segments.begin(),
                 m_segments.end(),
                 std::back_inserter(s),
                 [] (CurveSegmentModel* obj) { return obj->selection.get(); });
    std::copy_if(m_points.begin(),
                 m_points.end(),
                 std::back_inserter(s),
                 [] (CurvePointModel* obj) { return obj->selection.get(); });
    return s;
}

void CurveModel::setSelection(const Selection &s)
{
    for(auto& elt : m_segments)
        elt->selection.set(s.contains(elt));
    for(auto& elt : m_points)
        elt->selection.set(s.contains(elt));
}


void CurveModel::clear()
{
    qDeleteAll(m_segments);
    m_segments.clear();
    qDeleteAll(m_points);
    m_points.clear();
}


const QVector<CurveSegmentModel*>&CurveModel::segments() const
{
    return m_segments;
}

const QVector<CurvePointModel *> &CurveModel::points() const
{
    return m_points;
}

void CurveModel::addPoint(CurvePointModel *pt)
{
    m_points.append(pt);

    emit pointAdded(pt);
}

void CurveModel::removePoint(CurvePointModel *pt)
{
    auto index = m_points.indexOf(pt);
    if(index >= 0)
    {
        m_points.remove(index);
    }

    emit pointRemoved(pt);
    delete pt;
}
