#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveView.hpp"
#include "CurveSegmentModel.hpp"
#include "CurvePointModel.hpp"
#include "CurvePointView.hpp"
#include "CurveSegmentView.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include "OngoingCommandState.hpp"


#include "MovePointCommandObject.hpp"
#include "MoveSegmentCommandObject.hpp"

#include "StateMachine/CurveStateMachine.hpp"

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view):
    m_model{model},
    m_view{view}
{
    // For each segment in the model, create a segment and relevant points in the view.
    // If the segment is linked to another, the point is shared.
    setupView();
    setupSignals();
    m_sm = new CurveStateMachine(*this, this);
}

CurveModel* CurvePresenter::model() const
{
    return m_model;
}

CurveView& CurvePresenter::view() const
{
    return *m_view;
}

QPointF CurvePresenter::pressedPoint() const
{
    return m_currentScenePoint;
}

QPointF myscale(const QPointF& first, const QSizeF& second)
{
    return {first.x() * second.width(), (1. - first.y()) * second.height()};
}

void CurvePresenter::setRect(const QRectF& rect)
{
    m_view->setRect(rect);

    // Positions
    for(CurvePointView* curve_pt : m_points)
    {
        setPos(curve_pt);
    }

    for(CurveSegmentView* curve_segt : m_segments)
    {
        setPos(curve_segt);
    }
}

void CurvePresenter::setPos(CurvePointView * point)
{
    auto size = m_view->boundingRect().size();
    // Get the previous or next segment. There has to be at least one.
    if(point->model().previous())
    {
        auto it = std::find(m_model->segments().begin(),
                            m_model->segments().end(),
                            point->model().previous());
        Q_ASSERT(it != m_model->segments().end());

        point->setPos(myscale((*it)->end(), size));
    }
    else if(point->model().following())
    {
        auto it = std::find(m_model->segments().begin(),
                            m_model->segments().end(),
                            point->model().following());
        Q_ASSERT(it != m_model->segments().end());

        point->setPos(myscale((*it)->start(), size));
    }
}

void CurvePresenter::setPos(CurveSegmentView * segment)
{
    auto rect = m_view->boundingRect();
    // Pos is the top-left corner of the segment
    // Width is from begin to end
    // Height is the height of the curve since the segment can do anything in-between.
    double startx, endx;
    startx = segment->model().start().x() * rect.width();
    endx = segment->model().end().x() * rect.width();
    segment->setPos({startx, 0});
    segment->setRect({0., 0., endx - startx, rect.height()});

}

void CurvePresenter::setupSignals()
{
    connect(m_model, &CurveModel::segmentAdded, this,
            [&] (CurveSegmentModel* segment)
    {
        // Create a segment
        auto seg_view = new CurveSegmentView{segment, m_view};
        m_segments.push_back(seg_view);

        setPos(seg_view);
    });

    connect(m_model, &CurveModel::pointAdded, this,
            [&] (CurvePointModel* point)
    {
        // Create a segment
        auto pt_view = new CurvePointView{point, m_view};
        m_points.push_back(pt_view);

        setPos(pt_view);
    });

    connect(m_model, &CurveModel::pointRemoved, this,
            [&] (CurvePointModel* m)
    {
        auto it = std::find_if(
                    m_points.begin(),
                    m_points.end(),
                    [&] (CurvePointView* pt) { return &pt->model() == m; });
        auto val = *it;

        m_points.removeOne(val);
        delete val;
    });
    connect(m_model, &CurveModel::segmentRemoved, this,
            [&] (CurveSegmentModel* m)

    {
        auto it = std::find_if(
                    m_segments.begin(),
                    m_segments.end(),
                    [&] (CurveSegmentView* segment) { return &segment->model() == m; });
        auto val = *it;

        m_segments.removeOne(val);
        delete val;
    });
}

void CurvePresenter::setupView()
{
    for(CurveSegmentModel* segment : m_model->segments())
    {
        // Create a segment
        auto seg_view = new CurveSegmentView{segment, m_view};
        m_segments.push_back(seg_view);
    }

    for(CurvePointModel* pt : m_model->points())
    {
        // Create a point
        auto pt_view = new CurvePointView{pt, m_view};
        m_points.push_back(pt_view);
    }
}

void CurvePresenter::setupStateMachine()
{

}

