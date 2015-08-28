#include "CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include <boost/range/algorithm.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>


CurveModel::CurveModel(const Id<CurveModel>& id, QObject* parent):
    IdentifiedObject<CurveModel>(id, "CurveModel", parent)
{
}

CurveModel* CurveModel::clone(
        const Id<CurveModel>& id,
        QObject* parent)
{
    auto cm = new CurveModel{id, parent};
    for(const auto& segment : m_segments)
    {
        cm->addSegment(segment.clone(segment.id(), cm));
    }
    return cm;
}

void CurveModel::addSegment(CurveSegmentModel* m)
{
    m->setParent(this);
    m_segments.insert(m);

    emit segmentAdded(*m);

    // Add points if necessary
    // If there is an existing previous segment, its end point also exists
    if(m->previous())
    {
        auto previousSegment = std::find_if(m_segments.begin(), m_segments.end(),
                    [&] (const auto& seg) { return seg.following() == m->id(); });
        if(previousSegment != m_segments.end())
        {
            auto thePt = std::find_if(m_points.begin(), m_points.end(),
                        [&] (CurvePointModel* pt) { return pt->previous() == (*previousSegment).id(); });

            if(thePt != m_points.end())
            {
                (*thePt)->setFollowing(m->id());
            }
            else
            {
                auto pt = new CurvePointModel{getStrongId(m_points), this};
                pt->setFollowing(m->id());
                pt->setPos(m->start());
                addPoint(pt);
            }
        }
        else
        {
            auto pt = new CurvePointModel{getStrongId(m_points), this};
            pt->setFollowing(m->id());
            pt->setPos(m->start());
            addPoint(pt);
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->following() == m->id(); }))
    {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setFollowing(m->id());
        pt->setPos(m->start());
        addPoint(pt);
    }

    if(m->following())
    {
        auto followingSegment = std::find_if(m_segments.begin(), m_segments.end(),
                    [&] (const auto& seg) { return seg.previous() == m->id(); });
        if(followingSegment != m_segments.end())
        {
            auto thePt = std::find_if(m_points.begin(), m_points.end(),
                        [&] (CurvePointModel* pt) { return pt->following() == (*followingSegment).id(); });

            if(thePt != m_points.end())
            {
                (*thePt)->setPrevious(m->id());
            }
            else
            {
                auto pt = new CurvePointModel{getStrongId(m_points), this};
                pt->setPrevious(m->id());
                pt->setPos(m->end());
                addPoint(pt);
            }
        }
        else
        {
            auto pt = new CurvePointModel{getStrongId(m_points), this};
            pt->setPrevious(m->id());
            pt->setPos(m->end());
            addPoint(pt);
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->previous() == m->id(); }))
    {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setPrevious(m->id());
        pt->setPos(m->end());
        addPoint(pt);
    }
}


void CurveModel::removeSegment(CurveSegmentModel* m)
{
    m_segments.remove(m->id());

    emit segmentRemoved(m->id());

    for(CurvePointModel* pt : m_points)
    {
        if(pt->previous() == m->id())
        {
            pt->setPrevious(Id<CurveSegmentModel>{});
        }

        if(pt->following() == m->id())
        {
            pt->setFollowing(Id<CurveSegmentModel>{});
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
    for(const auto& elt : m_segments)
    {
        if(elt.selection.get())
            s.insert(&elt);
    }
    for(const auto& elt : m_points)
    {
        if(elt->selection.get())
            s.insert(elt);
    }

    return s;
}

void CurveModel::setSelection(const Selection &s)
{
    for(auto& elt : m_segments)
        elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : m_points)
        elt->selection.set(s.find(elt) != s.end());
}


void CurveModel::clear()
{
    emit cleared();

    qDeleteAll(m_segments.get());
    m_segments.clear();
    qDeleteAll(m_points);
    m_points.clear();
}




const QVector<CurvePointModel *> &CurveModel::points() const
{
    return m_points;
}

void CurveModel::addPoint(CurvePointModel *pt)
{
    m_points.append(pt);

    emit pointAdded(*pt);
}

void CurveModel::removePoint(CurvePointModel *pt)
{
    auto index = m_points.indexOf(pt);
    if(index >= 0)
    {
        m_points.remove(index);
    }

    emit pointRemoved(pt->id());
    delete pt;
}
