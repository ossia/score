#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveView.hpp"
#include "CurveSegmentModel.hpp"
#include "CurvePointView.hpp"
#include "CurveSegmentView.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include "OngoingCommandState.hpp"

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view):
    m_model{model},
    m_view{view}
{
    // For each segment in the model, create a segment and relevant points in the view.
    // If the segment is linked to another, the point is shared.
    setupView();


}

CurveModel* CurvePresenter::model() const
{
    return m_model;
}

QPointF CurvePresenter::pressedPoint() const
{
    return m_currentScenePoint;
}

void CurvePresenter::setupView()
{
    for(CurveSegmentModel* segment : m_model->segments())
    {
        // Create a segment
        auto seg_view = new CurveSegmentView{segment, m_view};
        m_segments.push_back(seg_view);

        // If there is no previous segment, we create a point.
        if(!segment->previous())
        {
            auto starting_pt_view = new CurvePointView{m_view};
            starting_pt_view->setFollowing(segment->id());
            m_points.append(starting_pt_view);
        }

        // We create a point in all cases at the end.
        auto ending_pt_view = new CurvePointView{m_view};
        ending_pt_view->setPrevious(segment->id());
        ending_pt_view->setFollowing(segment->following());
        m_points.append(ending_pt_view);
    }

    // Connections
    for(CurvePointView* curve_pt : m_points)
    {
        connect(curve_pt, &CurvePointView::pressed,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentPointView = curve_pt;

            m_sm.postEvent(new PressPoint_Event);
        });
        connect(curve_pt, &CurvePointView::moved,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentPointView = curve_pt;

            m_sm.postEvent(new MovePoint_Event);
        });
        connect(curve_pt, &CurvePointView::released,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentPointView = curve_pt;

            m_sm.postEvent(new ReleasePoint_Event);
        });
    }

    for(CurveSegmentView* curve_segt : m_segments)
    {
        connect(curve_segt, &CurveSegmentView::pressed,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentSegmentView = curve_segt;

            m_sm.postEvent(new PressSegment_Event);
        });
        connect(curve_segt, &CurveSegmentView::moved,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentSegmentView = curve_segt;

            m_sm.postEvent(new MoveSegment_Event);
        });
        connect(curve_segt, &CurveSegmentView::released,
                this, [&] (const QPointF& pt) {
            m_currentScenePoint = pt;
            m_currentSegmentView = curve_segt;

            m_sm.postEvent(new ReleaseSegment_Event);
        });
    }
}

#include "MovePointCommandObject.hpp"
#include "MoveSegmentCommandObject.hpp"

void CurvePresenter::setupStateMachine()
{
    auto waitState = new QState;
    m_sm.addState(waitState);
    m_sm.setInitialState(waitState);


}

