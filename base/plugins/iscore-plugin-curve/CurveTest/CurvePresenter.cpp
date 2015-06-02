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
    auto size = rect.size();
    // Positions
    for(CurvePointView* curve_pt : m_points)
    {
        // Get the previous or next segment. There has to be at least one.
        if(curve_pt->model().previous())
        {
            auto it = std::find(m_model->segments().begin(),
                                m_model->segments().end(),
                                curve_pt->model().previous());
            Q_ASSERT(it != m_model->segments().end());

            curve_pt->setPos(myscale((*it)->end(), size));
        }
        else if(curve_pt->model().following())
        {
            auto it = std::find(m_model->segments().begin(),
                                m_model->segments().end(),
                                curve_pt->model().following());
            Q_ASSERT(it != m_model->segments().end());

            curve_pt->setPos(myscale((*it)->start(), size));
        }
    }

    for(CurveSegmentView* curve_segt : m_segments)
    {
        // Pos is the top-left corner of the segment
        // Width is from begin to end
        // Height is the height of the curve since the segment can do anything in-between.
        double startx, endx;
        startx = curve_segt->model().start().x() * rect.width();
        endx = curve_segt->model().end().x() * rect.width();
        curve_segt->setPos({startx, 0});
        curve_segt->setRect({0., 0., endx - startx, rect.height()});

    }
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

    // Connections
    for(CurvePointView* curve_pt : m_points)
    {
        connect(curve_pt, &CurvePointView::pressed,
                this, [&] (const QPointF& pt) {
            m_view->pressed(pt);
        });
        connect(curve_pt, &CurvePointView::moved,
                this, [&] (const QPointF& pt) {
            m_view->moved(pt);
        });
        connect(curve_pt, &CurvePointView::released,
                this, [&] (const QPointF& pt) {
            m_view->released(pt);
        });
    }

    for(CurveSegmentView* curve_segt : m_segments)
    {
        connect(curve_segt, &CurveSegmentView::pressed,
                this, [&] (const QPointF& pt) {
            m_view->pressed(pt);
        });
        connect(curve_segt, &CurveSegmentView::moved,
                this, [&] (const QPointF& pt) {
            m_view->moved(pt);
        });
        connect(curve_segt, &CurveSegmentView::released,
                this, [&] (const QPointF& pt) {
            m_view->released(pt);
        });
    }
}

void CurvePresenter::setupStateMachine()
{

}

