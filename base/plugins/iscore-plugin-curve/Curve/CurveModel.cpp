#include "CurveModel.hpp"
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <boost/range/algorithm.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/DocumentContext.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <numeric>

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
        auto seg = segment.clone(segment.id(), cm);
        seg->setPrevious(segment.previous());
        seg->setFollowing(segment.following());
        cm->addSegment(seg);
    }
    return cm;
}


void CurveModel::addSortedSegment(CurveSegmentModel* m)
{
    insertSegment(m);

    // Add points if necessary
    // If there is an existing previous segment, its end point also exists
    auto createStartPoint = [&] () {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setFollowing(m->id());
        pt->setPos(m->start());
        addPoint(pt);
        return pt;
    };
    auto createEndPoint = [&] () {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setPrevious(m->id());
        pt->setPos(m->end());
        addPoint(pt);
        return pt;
    };

    if(!m->previous())
    {
        createStartPoint();
    }
    else
    {
        // The previous segment has already been inserted,
        // hence the previous point is present.
        m_points.back()->setFollowing(m->id());
    }

    createEndPoint();
}

void CurveModel::addSegment(CurveSegmentModel* m)
{
    insertSegment(m);

    // Add points if necessary
    // If there is an existing previous segment, its end point also exists
    auto createStartPoint = [&] () {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setFollowing(m->id());
        pt->setPos(m->start());
        addPoint(pt);
        return pt;
    };
    auto createEndPoint = [&] () {
        auto pt = new CurvePointModel{getStrongId(m_points), this};
        pt->setPrevious(m->id());
        pt->setPos(m->end());
        addPoint(pt);
        return pt;
    };

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
                // The previous segments and points both exist
                (*thePt)->setFollowing(m->id());
            }
            else
            {
                // The previous segment exists but not the end point.
                auto pt = createStartPoint();
                pt->setPrevious((*previousSegment).id());
            }
        }
        else // The previous segment has not yet been added.
        {
            createStartPoint();
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->following() == m->id(); }))
    {
        createStartPoint();
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
                auto pt = createEndPoint();
                pt->setFollowing((*followingSegment).id());
            }
        }
        else
        {
            createEndPoint();
        }
    }
    else if(std::none_of(m_points.begin(), m_points.end(),
                    [&] (CurvePointModel* pt)
                    { return pt->previous() == m->id(); }))
    {
        // Note : if one day a buggy case happens here, check that set following/previous
        // are correctly set after cloning the segment.
        createEndPoint();
    }
}


void CurveModel::insertSegment(CurveSegmentModel* m)
{
    m->setParent(this);
    m_segments.insert(m);

    // TODO have indexes on the points with the start and end
    // curve segments
    connect(m, &CurveSegmentModel::startChanged, this, [=] () {
        for(CurvePointModel* pt : m_points)
        {
            if(pt->following() == m->id())
            {
                pt->setPos(m->start());
                break;
            }
        }
    });
    connect(m, &CurveSegmentModel::endChanged, this, [=] () {
        for(CurvePointModel* pt : m_points)
        {
            if(pt->previous() == m->id())
            {
                pt->setPos(m->end());
                break;
            }
        }
    });

    emit segmentAdded(*m);
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

std::vector<CurveSegmentData> CurveModel::toCurveData() const
{
    std::vector<CurveSegmentData> dat;
    dat.reserve(m_segments.size());
    for(const auto& seg : m_segments)
    {
        dat.push_back(seg.toSegmentData());
    }

    return dat;
}

void CurveModel::fromCurveData(const std::vector<CurveSegmentData>& curve)
{
    this->blockSignals(true);
    clear();

    auto& context = iscore::IDocument::documentContext(*this).app;
    auto& csl = context.components.factory<DynamicCurveSegmentList>();
    CurveSegmentOrderedMap map(curve.begin(), curve.end());
    for(const auto& elt : map.get<Segments::Ordered>())
    {
        addSortedSegment(createCurveSegment(csl, elt, this));
    }
    this->blockSignals(false);
    emit curveReset();
    emit changed();
}

Selection CurveModel::selectedChildren() const
{
    Selection s;
    for(const auto& elt : m_segments)
    {
        if(elt.selection.get())
            s.append(&elt);
    }
    for(const auto& elt : m_points)
    {
        if(elt->selection.get())
            s.append(elt);
    }

    return s;
}

void CurveModel::setSelection(const Selection &s)
{
    // OPTIMIZEME
    for(auto& elt : m_segments)
        elt.selection.set(s.contains(&elt));
    for(auto& elt : m_points)
        elt->selection.set(s.contains(elt));
}

void CurveModel::clear()
{
    emit cleared();

    auto segs = m_segments;
    m_segments.clear();
    for(auto& seg : segs)
        seg.deleteLater();

    auto pts = m_points;
    m_points.clear();
    for(auto pt : pts)
        pt->deleteLater();
}




const std::vector<CurvePointModel *> &CurveModel::points() const
{
    return m_points;
}

void CurveModel::addPoint(CurvePointModel *pt)
{
    m_points.push_back(pt);

    emit pointAdded(*pt);
}

void CurveModel::removePoint(CurvePointModel *pt)
{
    auto it = find(m_points, pt);
    if(it != m_points.end())
    {
        m_points.erase(it);
    }

    emit pointRemoved(pt->id());
    delete pt;
}
